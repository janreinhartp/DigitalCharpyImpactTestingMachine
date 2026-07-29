#include "test_manager.h"
#include "bsp_angle.h"
#include "bsp_extra.h"
#include "bsp_rtc.h"
#include "bsp_servo.h"
#include "audio_manager.h"
#include "data_logger.h"
#include "dbtt_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <math.h>

#define TM_TAG "TEST_MGR"
#define TM_INFO(fmt, ...) ESP_LOGI(TM_TAG, fmt, ##__VA_ARGS__)
#define TM_ERROR(fmt, ...) ESP_LOGE(TM_TAG, fmt, ##__VA_ARGS__)

/* Angle stability detection thresholds */
#define SETTLE_TOLERANCE_DEG   2.0f    /* retained for reference, no longer used in sampling */
#define SETTLE_DURATION_MS    1000     /* retained for reference, no longer used in sampling */
#define SAMPLING_PERIOD_MS      20     /* 50 Hz — ADC does 16 reads/call (~5 ms work)        */
#define SWING_THRESHOLD_DEG   30.0f   /* Pendulum must move this far from release angle      *
                                       * before minimum tracking is enabled                  */
#define RECOVERY_THRESHOLD_DEG 5.0f   /* Once angle rises this far above the minimum the     *
                                       * swing peak is considered found — record and break   */
#define MEASURE_TIMEOUT_MS    30000   /* If no recovery detected within this time the         *
                                       * specimen is considered unbroken — record and stop   */

/* Actuator pulse duration */
#define ACTUATOR_PULSE_MS      200

/* Homing sequence timing */
#define HOMED_FORWARD_MS            3000   /* Forward travel (ms) after hitting bottom endstop  */
#define LATCH_SETTLE_MS             6000   /* Settle time (ms) after closing release latch      */
#define SAFETY_LOCK_PRE_REVERSE_MS  8000   /* Hold safety lock engaged (ms) before each reverse */

/* NVS storage keys */
#define NVS_NAMESPACE      "charpy"
#define NVS_KEY_MASS       "mass"
#define NVS_KEY_ARM_LEN    "arm_len"
#define NVS_KEY_REL_ANG    "rel_ang"
#define NVS_KEY_ZERO_OFF   "zero_off"
#define NVS_KEY_SCALE      "scale"
#define NVS_KEY_BRAKE_ANG  "brake_ang"
#define NVS_KEY_BRAKE_HOLD "brake_hld"
#define NVS_KEY_LATCH_MS   "latch_ms"

static test_state_t current_state     = TEST_STATE_IDLE;
static pendulum_config_t pendulum_cfg;
static float    brake_target_angle    = 90.0f;
static uint32_t brake_hold_ms         = 3000;
static uint32_t latch_open_ms         = 3000;   /* ms to hold release open during homing */
static specimen_info_t current_specimen;
static test_result_t current_result;
static float live_angle = 0.0f;
static test_state_cb_t state_change_cb = NULL;
static TaskHandle_t sampling_task_handle = NULL;
static volatile bool sampling_active = false;

/* Homing sequence resources */
static SemaphoreHandle_t s_bot_sem      = NULL;   /* signals bottom endstop press */
static TaskHandle_t      s_homing_task  = NULL;
static TaskHandle_t      s_arm_mon_task = NULL;
static char s_detail_text[96]           = "";     /* live sub-step description for UI */

/* Forward declarations — defined later in this file */
static void set_state(test_state_t new_state);
static void angle_sampling_task(void *arg);

/* Retract the brake servo to home (90°) after holding at brake position */
static void brake_retract_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(brake_hold_ms));
    servo_jog_to_angle(90.0f, 2000);
    TM_INFO("Brake servo retracting to home");
    vTaskDelete(NULL);
}

/* Fires the release actuator from a dedicated task so the LVGL/UI task is
 * never blocked. The actuator stays ON for the entire swing and is turned
 * off by angle_sampling_task once the pendulum has settled. */
static void release_fire_task(void *arg)
{
    (void)arg;
    TM_INFO("[RELEASE] Actuator ON — will stay open until pendulum settles");
    release_set(true);

    set_state(TEST_STATE_MEASURING);
    sampling_active = true;
    xTaskCreate(angle_sampling_task, "angle_sample", 4096, NULL,
                tskIDLE_PRIORITY + 3, &sampling_task_handle);

    vTaskDelete(NULL);
}

/* ── Internal callback: bottom endstop pressed ──────────────────────────── */
static void endstop_bot_triggered_internal(bool pressed)
{
    if (!pressed) return;
    TM_INFO("[ENDSTOP] Bottom endstop triggered");
    if (s_bot_sem != NULL) {
        xSemaphoreGive(s_bot_sem);
    }
}

/* ── Arming angle monitor task ──────────────────────────────────────────── *
 * Polls the angle sensor during ARMING; stops the motor and transitions to
 * ARMED when the configured release angle (soft limit) is reached.
 * The top endstop acts as a hard limit and can also trigger the transition.
 */
static void arming_angle_monitor_task(void *arg)
{
    (void)arg;
    TM_INFO("[ARMING] Angle monitor started — target %.1f° (soft limit)",
            pendulum_cfg.release_angle_deg);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Lifting to %.0f° — angle monitor active",
             pendulum_cfg.release_angle_deg);
    while (current_state == TEST_STATE_ARMING) {
        float angle;
        if (angle_read_degrees(&angle) == ESP_OK) {
            live_angle = angle;
            snprintf(s_detail_text, sizeof(s_detail_text),
                     "Lifting arm: %.1f° / %.1f° target",
                     angle, pendulum_cfg.release_angle_deg);
            if (fabsf(angle) >= pendulum_cfg.release_angle_deg) {
                /* Soft limit: target degree reached */
                if (current_state == TEST_STATE_ARMING) {   /* re-check after read */
                    motor_forward_set(false);
                    safety_lock_set(true);
                    current_result.release_angle_deg = angle;
                    TM_INFO("[ARMING] Target angle %.1f° reached — motor OFF, ARMED", angle);
                    snprintf(s_detail_text, sizeof(s_detail_text),
                             "Armed at %.1f°", angle);
                    audio_manager_play(SOUND_ARMED);
                    set_state(TEST_STATE_ARMED);
                }
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));   /* 100 Hz polling */
    }
    s_arm_mon_task = NULL;
    vTaskDelete(NULL);
}

/* ── Homing & preparation task ──────────────────────────────────────────── *
 * Runs the full sequence before an actual arm-and-release:
 *   1. Reverse motor → bottom endstop (homing)
 *   2. Forward motor for HOMED_FORWARD_MS
 *   3. Open release latch for latch_open_ms
 *   4. Reverse motor → bottom endstop again (return home, catches pendulum)
 *   5. Close release latch (latch onto pendulum)
 *   6. Forward motor toward test position (ARMING); stopped by top endstop
 *      or by arming_angle_monitor_task when the set degree is reached.
 */
static void homing_task(void *arg)
{
    (void)arg;
    TM_INFO("[HOMING] Sequence started");

    /* ── Step 1: Reverse to bottom endstop ──────────────────────────────── */
    set_state(TEST_STATE_HOMING);
    TM_INFO("[HOMING 1/5] Deactivating safety lock — holding %d ms before reverse",
            SAFETY_LOCK_PRE_REVERSE_MS);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 1/5: Safety lock released — waiting %d s...",
             SAFETY_LOCK_PRE_REVERSE_MS / 1000);
    safety_lock_set(false);
    vTaskDelay(pdMS_TO_TICKS(SAFETY_LOCK_PRE_REVERSE_MS));

    TM_INFO("[HOMING 1/5] Reversing motor — waiting for bottom endstop");
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 1/5: Reversing to home position...");
    motor_reverse_set(true);
    xSemaphoreTake(s_bot_sem, portMAX_DELAY);
    motor_reverse_set(false);
    TM_INFO("[HOMING 1/5] Bottom endstop reached — motor stopped");

    /* ── Step 1b: Index forward for 2 s ──────────────────────────────────── */
    set_state(TEST_STATE_HOMED_FWD);
    TM_INFO("[HOMING 2/5] Moving forward for %d ms (indexing)", HOMED_FORWARD_MS);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 2/5: Indexing forward %d s...",
             HOMED_FORWARD_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(200));
    safety_lock_set(true);
    motor_forward_set(true);
    vTaskDelay(pdMS_TO_TICKS(HOMED_FORWARD_MS));
    motor_forward_set(false);
    TM_INFO("[HOMING 2/5] Forward index complete");

    /* ── Step 2: Open the release latch for latch_open_ms ───────────────── */
    set_state(TEST_STATE_LATCH_OPENING);
    TM_INFO("[HOMING 3/5] Opening release latch for %lu ms", (unsigned long)latch_open_ms);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 3/5: Release latch open — holding %lu s...",
             (unsigned long)(latch_open_ms / 1000));
    release_set(true);
    vTaskDelay(pdMS_TO_TICKS(latch_open_ms));
    /* NOTE: latch stays OPEN — it will close only after bottom endstop fires */
    TM_INFO("[HOMING 3/5] Latch open period done — keeping open while reversing");

    /* ── Step 3: Reverse to bottom endstop to catch the pendulum ────────── */
    set_state(TEST_STATE_RETURNING_HOME);
    TM_INFO("[HOMING 4/5] Deactivating safety lock — holding %d ms before reverse",
            SAFETY_LOCK_PRE_REVERSE_MS);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 4/5: Safety lock released — waiting %d s...",
             SAFETY_LOCK_PRE_REVERSE_MS / 1000);
    safety_lock_set(false);
    vTaskDelay(pdMS_TO_TICKS(SAFETY_LOCK_PRE_REVERSE_MS));

    TM_INFO("[HOMING 4/5] Reversing to home — latch still open, waiting to catch pendulum");
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 4/5: Returning to catch pendulum (latch open)...");
    motor_reverse_set(true);
    xSemaphoreTake(s_bot_sem, portMAX_DELAY);
    motor_reverse_set(false);
    /* Close latch NOW — arm is at home, pendulum is in catch position */
    release_set(false);
    TM_INFO("[HOMING 4/5] Bottom endstop reached — latch CLOSED, pendulum latched");

    /* ── Step 4: Release is already closed — latch engages pendulum ──────── */
    set_state(TEST_STATE_LATCHED);
    TM_INFO("[HOMING 5/5] Latch engaged — settling %d ms before arming", LATCH_SETTLE_MS);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Step 5/5: Pendulum latched — settling %d ms...",
             LATCH_SETTLE_MS);
    safety_lock_set(true);
    vTaskDelay(pdMS_TO_TICKS(LATCH_SETTLE_MS));

    /* ── Step 5: Arm — move forward to test position ─────────────────────── */
    set_state(TEST_STATE_ARMING);
    TM_INFO("[HOMING 5/5] ARMING — lifting arm to %.1f° (hard limit: top endstop)",
            pendulum_cfg.release_angle_deg);
    snprintf(s_detail_text, sizeof(s_detail_text),
             "Lifting arm to %.0f\xc2\xb0...",
             pendulum_cfg.release_angle_deg);
    motor_forward_set(true);

    /* Start angle monitor (soft limit); top endstop is the hard limit */
    xTaskCreate(arming_angle_monitor_task, "arm_angle", 4096, NULL,
                configMAX_PRIORITIES - 3, &s_arm_mon_task);

    /* Clean up and exit — hardware is now running toward the test position */
    s_homing_task = NULL;
    vTaskDelete(NULL);
}

static void set_state(test_state_t new_state)
{
    test_state_t old = current_state;
    current_state = new_state;
    TM_INFO("State: %s -> %s", test_manager_state_name(old), test_manager_state_name(new_state));
    if (state_change_cb != NULL) {
        state_change_cb(old, new_state);
    }
}

/* High-priority angle sampling task
 * Algorithm:
 *   1. Record baseline angle at the moment of release (first sample).
 *   2. Wait for the pendulum to swing SWING_THRESHOLD_DEG away from baseline.
 *   3. Track the running minimum angle (lowest / most-negative value).
 *   4. Once the angle rises RECOVERY_THRESHOLD_DEG above that minimum the
 *      swing peak has been passed — record min_angle as the impact result
 *      and stop.  This gives the correct Charpy β angle for energy calc.
 */
static void angle_sampling_task(void *arg)
{
    (void)arg;
    float start_angle  = 0.0f;
    float min_angle    = 0.0f;
    bool  first_read   = true;
    bool  swing_active = false;   /* true once pendulum has passed SWING_THRESHOLD */
    uint32_t last_log_ms = 0;
    uint32_t task_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    TM_INFO("Angle sampling started — swing gate %.1f°, recovery %.1f°, timeout %d s",
            SWING_THRESHOLD_DEG, RECOVERY_THRESHOLD_DEG, MEASURE_TIMEOUT_MS / 1000);

    while (sampling_active) {
        float angle;
        if (angle_read_degrees(&angle) == ESP_OK) {
            live_angle = angle;

            /* Log at ~10 Hz */
            uint32_t now_log_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ((now_log_ms - last_log_ms) >= 100) {
                if (swing_active) {
                    TM_INFO("Angle: %.2f°  (min so far: %.2f°)", angle, min_angle);
                } else {
                    TM_INFO("Angle: %.2f° [waiting for swing]", angle);
                }
                last_log_ms = now_log_ms;
            }

            /* ── Timeout: specimen not broken ──────────────────────────────────── */
            uint32_t elapsed_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - task_start_ms;
            if (elapsed_ms >= MEASURE_TIMEOUT_MS) {
                TM_ERROR("Measurement timeout (%d s) — specimen likely NOT BROKEN (min=%.1f°)",
                         MEASURE_TIMEOUT_MS / 1000, min_angle);
                sampling_active = false;
                release_set(false);

                /* Record with whatever minimum we observed; mark in notes */
                current_result.final_angle_deg = min_angle;
                current_result.energy_joules = charpy_calc_energy(
                    pendulum_cfg.mass_kg,
                    pendulum_cfg.arm_length_m,
                    current_result.release_angle_deg,
                    min_angle);

                rtc_datetime_t dt_to;
                if (rtc_get_datetime(&dt_to) == ESP_OK) {
                    rtc_format_timestamp(&dt_to, current_result.timestamp,
                                        sizeof(current_result.timestamp));
                }

                current_result.specimen = current_specimen;
                strncat(current_result.specimen.notes, " TIMEOUT-NOT-CUT",
                        sizeof(current_result.specimen.notes)
                        - strlen(current_result.specimen.notes) - 1);

                data_logger_write_result(&current_result);
                if (dbtt_manager_is_active()) dbtt_manager_record_result(&current_result);

                servo_set_angle(brake_target_angle);
                xTaskCreate(brake_retract_task, "brake_ret", 4096, NULL, 3, NULL);
                audio_manager_play(SOUND_COMPLETE);
                set_state(TEST_STATE_COMPLETE);
                break;
            }

            if (first_read) {
                start_angle = angle;
                min_angle   = angle;
                first_read  = false;
                /* Use the angle at the instant of release as α — more accurate
                 * than the arming endstop reading because the heavy pendulum
                 * may cause slight backward slip while stationary. */
                current_result.release_angle_deg = angle;
                TM_INFO("Release angle (at click): %.2f° — waiting for %.1f° swing",
                        angle, SWING_THRESHOLD_DEG);

            } else if (!swing_active) {
                /* Gate: wait until pendulum has moved far enough */
                if (fabsf(angle - start_angle) >= SWING_THRESHOLD_DEG) {
                    swing_active = true;
                    min_angle    = angle;
                    TM_INFO("Swing detected at %.1f° (moved %.1f°) — tracking minimum",
                            angle, fabsf(angle - start_angle));
                }

            } else {
                /* Track the lowest (most negative) angle reached */
                if (angle < min_angle) {
                    min_angle = angle;
                }

                /* Recovery: angle has risen RECOVERY_THRESHOLD above the minimum */
                if (angle - min_angle >= RECOVERY_THRESHOLD_DEG) {
                    TM_INFO("Peak minimum: %.2f° (current: %.2f°) — recording result",
                            min_angle, angle);
                    sampling_active = false;

                    release_set(false);
                    TM_INFO("[RELEASE] Actuator OFF");

                    /* Use the peak minimum as the post-impact angle β */
                    current_result.final_angle_deg = min_angle;
                    current_result.energy_joules = charpy_calc_energy(
                        pendulum_cfg.mass_kg,
                        pendulum_cfg.arm_length_m,
                        current_result.release_angle_deg,
                        min_angle);

                    /* Timestamp */
                    rtc_datetime_t dt;
                    if (rtc_get_datetime(&dt) == ESP_OK) {
                        rtc_format_timestamp(&dt, current_result.timestamp,
                                            sizeof(current_result.timestamp));
                    }

                    /* Auto-save */
                    current_result.specimen = current_specimen;
                    esp_err_t save_err = data_logger_write_result(&current_result);
                    if (save_err != ESP_OK) {
                        TM_ERROR("Auto-save failed: %s", esp_err_to_name(save_err));
                    } else {
                        TM_INFO("Result saved: α=%.1f°  β=%.1f°  E=%.3f J",
                                current_result.release_angle_deg, min_angle,
                                current_result.energy_joules);
                    }

                    if (dbtt_manager_is_active()) {
                        dbtt_manager_record_result(&current_result);
                        TM_INFO("DBTT result recorded");
                    }

                    servo_set_angle(brake_target_angle);
                    TM_INFO("Brake engaged at %.1f°", brake_target_angle);
                    xTaskCreate(brake_retract_task, "brake_ret", 4096, NULL, 3, NULL);
                    audio_manager_play(SOUND_COMPLETE);
                    set_state(TEST_STATE_COMPLETE);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD_MS));
    }

    TM_INFO("Angle sampling stopped");
    vTaskDelete(NULL);
}

static esp_err_t load_config_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        TM_INFO("No saved config in NVS, using defaults");
        pendulum_cfg = charpy_default_config();
        return ESP_OK;
    }

    /* NVS doesn't support float directly — store as int32 (value * 1000) */
    int32_t val;
    if (nvs_get_i32(nvs, NVS_KEY_MASS, &val) == ESP_OK)
        pendulum_cfg.mass_kg = (float)val / 1000.0f;
    if (nvs_get_i32(nvs, NVS_KEY_ARM_LEN, &val) == ESP_OK)
        pendulum_cfg.arm_length_m = (float)val / 1000.0f;
    if (nvs_get_i32(nvs, NVS_KEY_REL_ANG, &val) == ESP_OK)
        pendulum_cfg.release_angle_deg = (float)val / 1000.0f;

    /* Load zero offset */
    uint16_t offset;
    if (nvs_get_u16(nvs, NVS_KEY_ZERO_OFF, &offset) == ESP_OK)
        angle_set_zero_offset(offset);

    /* Load pulley scale factor */
    if (nvs_get_i32(nvs, NVS_KEY_SCALE, &val) == ESP_OK)
        angle_set_scale((float)val / 10000.0f);

    /* Load brake servo target angle */
    if (nvs_get_i32(nvs, NVS_KEY_BRAKE_ANG, &val) == ESP_OK)
        brake_target_angle = (float)val / 1000.0f;

    /* Load brake hold time */
    uint32_t hold;
    if (nvs_get_u32(nvs, NVS_KEY_BRAKE_HOLD, &hold) == ESP_OK)
        brake_hold_ms = hold;

    /* Load latch open time */
    uint32_t latch_ms;
    if (nvs_get_u32(nvs, NVS_KEY_LATCH_MS, &latch_ms) == ESP_OK)
        latch_open_ms = latch_ms;

    nvs_close(nvs);
    TM_INFO("Config loaded: mass=%.3fkg, arm=%.3fm, release=%.1f°",
            pendulum_cfg.mass_kg, pendulum_cfg.arm_length_m, pendulum_cfg.release_angle_deg);
    return ESP_OK;
}

static esp_err_t save_config_to_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        TM_ERROR("Failed to open NVS for writing");
        return err;
    }

    nvs_set_i32(nvs, NVS_KEY_MASS, (int32_t)(pendulum_cfg.mass_kg * 1000.0f));
    nvs_set_i32(nvs, NVS_KEY_ARM_LEN, (int32_t)(pendulum_cfg.arm_length_m * 1000.0f));
    nvs_set_i32(nvs, NVS_KEY_REL_ANG, (int32_t)(pendulum_cfg.release_angle_deg * 1000.0f));
    nvs_set_u16(nvs, NVS_KEY_ZERO_OFF, angle_get_zero_offset());
    nvs_set_i32(nvs, NVS_KEY_SCALE,    (int32_t)(angle_get_signed_scale() * 10000.0f));
    nvs_set_i32(nvs, NVS_KEY_BRAKE_ANG, (int32_t)(brake_target_angle * 1000.0f));
    nvs_set_u32(nvs, NVS_KEY_BRAKE_HOLD, brake_hold_ms);
    nvs_set_u32(nvs, NVS_KEY_LATCH_MS,  latch_open_ms);

    err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t test_manager_init(const pendulum_config_t *config)
{
    if (config != NULL) {
        pendulum_cfg = *config;
    } else {
        pendulum_cfg = charpy_default_config();
        load_config_from_nvs();
    }

    memset(&current_specimen, 0, sizeof(current_specimen));
    memset(&current_result, 0, sizeof(current_result));
    current_state = TEST_STATE_IDLE;

    /* Register endstop callbacks internally */
    endstop_register_callback(test_manager_endstop_triggered);
    endstop_bot_register_callback(endstop_bot_triggered_internal);

    TM_INFO("Test manager initialized");
    return ESP_OK;
}

test_state_t test_manager_get_state(void)
{
    return current_state;
}

const char *test_manager_state_name(test_state_t state)
{
    switch (state) {
        case TEST_STATE_IDLE:           return "IDLE";
        case TEST_STATE_SPECIMEN_ENTRY: return "SPECIMEN_ENTRY";
        case TEST_STATE_HOMING:         return "HOMING";
        case TEST_STATE_HOMED_FWD:      return "HOMED_FWD";
        case TEST_STATE_LATCH_OPENING:  return "LATCH_OPENING";
        case TEST_STATE_RETURNING_HOME: return "RETURNING_HOME";
        case TEST_STATE_LATCHED:        return "LATCHED";
        case TEST_STATE_ARMING:         return "ARMING";
        case TEST_STATE_ARMED:          return "ARMED";
        case TEST_STATE_RELEASED:       return "RELEASED";
        case TEST_STATE_MEASURING:      return "MEASURING";
        case TEST_STATE_COMPLETE:       return "COMPLETE";
        case TEST_STATE_LOGGING:        return "LOGGING";
        case TEST_STATE_ERROR:          return "ERROR";
        default:                        return "UNKNOWN";
    }
}

esp_err_t test_manager_start_new(void)
{
    if (current_state != TEST_STATE_IDLE) {
        TM_ERROR("Cannot start new test from state %s", test_manager_state_name(current_state));
        return ESP_ERR_INVALID_STATE;
    }

    memset(&current_specimen, 0, sizeof(current_specimen));
    memset(&current_result, 0, sizeof(current_result));

    /* Set default dimensions */
    current_specimen.width_mm = 10.0f;
    current_specimen.height_mm = 10.0f;
    current_specimen.length_mm = 55.0f;

    set_state(TEST_STATE_SPECIMEN_ENTRY);
    return ESP_OK;
}

esp_err_t test_manager_arm(const specimen_info_t *specimen)
{
    if (current_state != TEST_STATE_SPECIMEN_ENTRY) {
        TM_ERROR("Cannot arm from state %s", test_manager_state_name(current_state));
        return ESP_ERR_INVALID_STATE;
    }

    if (specimen != NULL) {
        current_specimen = *specimen;
    }

    /* Create bottom-endstop semaphore used by the homing task */
    s_bot_sem = xSemaphoreCreateBinary();
    if (s_bot_sem == NULL) {
        TM_ERROR("Failed to create bottom endstop semaphore");
        return ESP_ERR_NO_MEM;
    }

    /* Launch the homing + preparation task */
    BaseType_t ret = xTaskCreate(homing_task, "homing", 4096, NULL,
                                 configMAX_PRIORITIES - 2, &s_homing_task);
    if (ret != pdPASS) {
        vSemaphoreDelete(s_bot_sem);
        s_bot_sem = NULL;
        TM_ERROR("Failed to create homing task");
        return ESP_ERR_NO_MEM;
    }

    TM_INFO("Homing sequence started");
    return ESP_OK;
}

void test_manager_endstop_triggered(bool pressed)
{
    (void)pressed;
    if (current_state != TEST_STATE_ARMING)
        return;

    /* Arm reached top (hard limit) — stop motor and engage safety lock */
    motor_forward_set(false);
    safety_lock_set(true);

    /* Signal the soft-limit angle monitor task to exit naturally.
     * Do NOT force-kill it with vTaskDelete(): if the task is blocked inside
     * adc_oneshot_read() when it is deleted, the ADC driver's internal arbiter
     * lock is never released and every subsequent ADC read times out.
     * The task's loop condition (current_state == TEST_STATE_ARMING) will cause
     * it to exit cleanly once set_state(ARMED) is called below. */
    s_arm_mon_task = NULL;   /* prevent double-free if task self-exits first */

    TM_INFO("Top endstop hit — motor OFF, safety lock ENGAGED");

    /* Read and record the actual release angle */
    float angle;
    if (angle_read_degrees(&angle) == ESP_OK) {
        current_result.release_angle_deg = angle;
        TM_INFO("Release angle: %.1f°", angle);
    } else {
        current_result.release_angle_deg = pendulum_cfg.release_angle_deg;
    }

    /* Play armed sound */
    audio_manager_play(SOUND_ARMED);

    set_state(TEST_STATE_ARMED);
}

esp_err_t test_manager_release(void)
{
    if (current_state != TEST_STATE_ARMED) {
        TM_ERROR("Cannot release from state %s", test_manager_state_name(current_state));
        return ESP_ERR_INVALID_STATE;
    }

    set_state(TEST_STATE_RELEASED);
    audio_manager_play(SOUND_RELEASED);

    /* Fire the actuator in a separate task — never block the LVGL/UI task */
    BaseType_t ret = xTaskCreate(release_fire_task, "release_fire", 4096, NULL,
                                 configMAX_PRIORITIES - 2, NULL);
    if (ret != pdPASS) {
        TM_ERROR("Failed to create release_fire_task");
        release_set(false);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t test_manager_abort(void)
{
    if (current_state == TEST_STATE_IDLE ||
        current_state == TEST_STATE_RELEASED ||
        current_state == TEST_STATE_MEASURING ||
        current_state == TEST_STATE_LOGGING) {
        TM_ERROR("Cannot abort from state %s", test_manager_state_name(current_state));
        return ESP_ERR_INVALID_STATE;
    }

    /* Kill homing task if it is blocked waiting for an endstop */
    if (s_homing_task != NULL) {
        TaskHandle_t h = s_homing_task;
        s_homing_task = NULL;
        vTaskDelete(h);
    }

    /* Kill arming angle monitor task if running */
    if (s_arm_mon_task != NULL) {
        TaskHandle_t h = s_arm_mon_task;
        s_arm_mon_task = NULL;
        vTaskDelete(h);
    }

    /* Release semaphore so no task is left blocking on it */
    if (s_bot_sem != NULL) {
        vSemaphoreDelete(s_bot_sem);
        s_bot_sem = NULL;
    }

    s_detail_text[0] = '\0';   /* clear sub-step display */

    /* Stop all hardware and engage safety lock */
    motor_forward_set(false);
    motor_reverse_set(false);
    release_set(false);
    safety_lock_set(true);
    sampling_active = false;

    audio_manager_play(SOUND_ABORT);

    set_state(TEST_STATE_IDLE);
    TM_INFO("Test aborted");
    return ESP_OK;
}

esp_err_t test_manager_save_result(void)
{
    if (current_state != TEST_STATE_COMPLETE) {
        TM_ERROR("Cannot save from state %s", test_manager_state_name(current_state));
        return ESP_ERR_INVALID_STATE;
    }

    set_state(TEST_STATE_LOGGING);

    /* Copy specimen info into result */
    current_result.specimen = current_specimen;

    /* Log to SD card */
    esp_err_t err = data_logger_write_result(&current_result);
    if (err != ESP_OK) {
        TM_ERROR("Failed to log result to SD card");
        audio_manager_play(SOUND_ERROR);
        set_state(TEST_STATE_ERROR);
        return err;
    }

    TM_INFO("Result saved: %.1f° / %.2f J", current_result.final_angle_deg, current_result.energy_joules);
    set_state(TEST_STATE_IDLE);
    return ESP_OK;
}

esp_err_t test_manager_discard_result(void)
{
    if (current_state != TEST_STATE_COMPLETE) {
        return ESP_ERR_INVALID_STATE;
    }

    TM_INFO("Result discarded");
    set_state(TEST_STATE_IDLE);
    return ESP_OK;
}

float test_manager_get_live_angle(void)
{
    return live_angle;
}

const test_result_t *test_manager_get_result(void)
{
    return &current_result;
}

const specimen_info_t *test_manager_get_specimen(void)
{
    return &current_specimen;
}

void test_manager_register_state_cb(test_state_cb_t cb)
{
    state_change_cb = cb;
}

esp_err_t test_manager_set_config(const pendulum_config_t *config)
{
    if (config == NULL)
        return ESP_ERR_INVALID_ARG;

    pendulum_cfg = *config;
    save_config_to_nvs();
    TM_INFO("Config updated: mass=%.3fkg, arm=%.3fm, release=%.1f°",
            pendulum_cfg.mass_kg, pendulum_cfg.arm_length_m, pendulum_cfg.release_angle_deg);
    return ESP_OK;
}

esp_err_t test_manager_save_config(void)
{
    return save_config_to_nvs();
}

const pendulum_config_t *test_manager_get_config(void)
{
    return &pendulum_cfg;
}

float test_manager_get_brake_angle(void)
{
    return brake_target_angle;
}

esp_err_t test_manager_set_brake_angle(float angle_deg)
{
    if (angle_deg < 0.0f || angle_deg > 180.0f)
        return ESP_ERR_INVALID_ARG;
    brake_target_angle = angle_deg;
    return save_config_to_nvs();
}

uint32_t test_manager_get_brake_hold_ms(void)
{
    return brake_hold_ms;
}

esp_err_t test_manager_set_brake_hold_ms(uint32_t ms)
{
    if (ms < 100 || ms > 30000)
        return ESP_ERR_INVALID_ARG;
    brake_hold_ms = ms;
    TM_INFO("Brake hold time set to %lu ms", (unsigned long)ms);
    return save_config_to_nvs();
}

uint32_t test_manager_get_latch_open_ms(void)
{
    return latch_open_ms;
}

esp_err_t test_manager_set_latch_open_ms(uint32_t ms)
{
    if (ms < 500 || ms > 30000)
        return ESP_ERR_INVALID_ARG;
    latch_open_ms = ms;
    TM_INFO("Latch open time set to %lu ms", (unsigned long)ms);
    return save_config_to_nvs();
}

const char *test_manager_get_detail_text(void)
{
    return s_detail_text;
}
