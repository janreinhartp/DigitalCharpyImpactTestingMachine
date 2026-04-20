#include "test_manager.h"
#include "bsp_angle.h"
#include "bsp_extra.h"
#include "bsp_rtc.h"
#include "audio_manager.h"
#include "data_logger.h"

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
#define SETTLE_TOLERANCE_DEG   0.5f    /* ±0.5° */
#define SETTLE_DURATION_MS     500     /* Stable for 500ms to confirm settled */
#define SAMPLING_PERIOD_MS     1       /* ~1kHz sampling */

/* Actuator pulse duration */
#define ACTUATOR_PULSE_MS      200

/* NVS storage keys */
#define NVS_NAMESPACE   "charpy"
#define NVS_KEY_MASS    "mass"
#define NVS_KEY_ARM_LEN "arm_len"
#define NVS_KEY_REL_ANG "rel_ang"
#define NVS_KEY_ZERO_OFF "zero_off"

static test_state_t current_state = TEST_STATE_IDLE;
static pendulum_config_t pendulum_cfg;
static specimen_info_t current_specimen;
static test_result_t current_result;
static float live_angle = 0.0f;
static test_state_cb_t state_change_cb = NULL;
static TaskHandle_t sampling_task_handle = NULL;
static volatile bool sampling_active = false;

static void set_state(test_state_t new_state)
{
    test_state_t old = current_state;
    current_state = new_state;
    TM_INFO("State: %s -> %s", test_manager_state_name(old), test_manager_state_name(new_state));
    if (state_change_cb != NULL) {
        state_change_cb(old, new_state);
    }
}

/* High-priority angle sampling task */
static void angle_sampling_task(void *arg)
{
    (void)arg;
    float prev_angle = 0.0f;
    uint32_t stable_start_ms = 0;
    bool first_read = true;

    TM_INFO("Angle sampling started");

    while (sampling_active) {
        float angle;
        if (angle_read_degrees(&angle) == ESP_OK) {
            live_angle = angle;

            if (first_read) {
                prev_angle = angle;
                stable_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                first_read = false;
            } else {
                float diff = fabsf(angle - prev_angle);
                if (diff > SETTLE_TOLERANCE_DEG) {
                    /* Still swinging — reset stability timer */
                    prev_angle = angle;
                    stable_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                } else {
                    /* Check if stable long enough */
                    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    if ((now_ms - stable_start_ms) >= SETTLE_DURATION_MS) {
                        /* Pendulum has settled */
                        TM_INFO("Pendulum settled at %.1f°", angle);
                        sampling_active = false;

                        /* Record final angle and calculate energy */
                        current_result.final_angle_deg = angle;
                        current_result.energy_joules = charpy_calc_energy(
                            pendulum_cfg.mass_kg,
                            pendulum_cfg.arm_length_m,
                            current_result.release_angle_deg,
                            angle);

                        /* Get timestamp */
                        rtc_datetime_t dt;
                        if (rtc_get_datetime(&dt) == ESP_OK) {
                            rtc_format_timestamp(&dt, current_result.timestamp, sizeof(current_result.timestamp));
                        }

                        /* Play completion sound */
                        audio_manager_play(SOUND_COMPLETE);

                        set_state(TEST_STATE_COMPLETE);
                    }
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

    set_state(TEST_STATE_ARMING);

    /* Turn on motor to lift arm */
    motor_relay_set(true);
    TM_INFO("Motor ON — lifting arm");

    return ESP_OK;
}

void test_manager_endstop_triggered(bool pressed)
{
    (void)pressed;
    if (current_state != TEST_STATE_ARMING)
        return;

    /* Arm reached top — stop motor */
    motor_relay_set(false);
    TM_INFO("Endstop hit — motor OFF, arm in position");

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

    /* Play release sound */
    audio_manager_play(SOUND_RELEASED);

    /* Fire actuator (brief pulse to release latch) */
    actuator_relay_set(true);
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_PULSE_MS));
    actuator_relay_set(false);

    /* Transition to MEASURING and start high-speed angle sampling */
    set_state(TEST_STATE_MEASURING);
    sampling_active = true;
    xTaskCreate(angle_sampling_task, "angle_sample", 4096, NULL, configMAX_PRIORITIES - 2, &sampling_task_handle);

    return ESP_OK;
}

esp_err_t test_manager_abort(void)
{
    if (current_state != TEST_STATE_ARMING &&
        current_state != TEST_STATE_ARMED &&
        current_state != TEST_STATE_SPECIMEN_ENTRY) {
        TM_ERROR("Cannot abort from state %s", test_manager_state_name(current_state));
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop all hardware */
    motor_relay_set(false);
    actuator_relay_set(false);
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

const pendulum_config_t *test_manager_get_config(void)
{
    return &pendulum_cfg;
}
