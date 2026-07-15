#include "bsp_servo.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

static mcpwm_timer_handle_t    s_timer      = NULL;
static mcpwm_oper_handle_t     s_operator   = NULL;
static mcpwm_cmpr_handle_t     s_comparator = NULL;
static mcpwm_gen_handle_t      s_generator  = NULL;

/* Jog task state */
static float              s_current_angle = 90.0f;
static TaskHandle_t       s_jog_task      = NULL;
static volatile bool      s_jog_cancel    = false;

typedef struct {
    float    target;
    uint32_t duration_ms;
} jog_args_t;

/* Map an angle in degrees to a pulse width in microseconds */
static inline uint32_t angle_to_pulse_us(float angle_deg)
{
    if (angle_deg < SERVO_MIN_ANGLE) angle_deg = SERVO_MIN_ANGLE;
    if (angle_deg > SERVO_MAX_ANGLE) angle_deg = SERVO_MAX_ANGLE;

    float ratio = (angle_deg - SERVO_MIN_ANGLE) / (SERVO_MAX_ANGLE - SERVO_MIN_ANGLE);
    return (uint32_t)(SERVO_MAX_PULSE_US - ratio * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US));
}

esp_err_t servo_init(void)
{
    esp_err_t err;

    /* --- Timer ----------------------------------------------------------- */
    mcpwm_timer_config_t timer_cfg = {
        .group_id      = 0,
        .clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMER_RES_HZ,
        .period_ticks  = SERVO_PERIOD_TICKS,
        .count_mode    = MCPWM_TIMER_COUNT_MODE_UP,
    };
    err = mcpwm_new_timer(&timer_cfg, &s_timer);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to create MCPWM timer: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Operator -------------------------------------------------------- */
    mcpwm_operator_config_t oper_cfg = {
        .group_id = 0,
    };
    err = mcpwm_new_operator(&oper_cfg, &s_operator);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to create MCPWM operator: %s", esp_err_to_name(err));
        return err;
    }

    err = mcpwm_operator_connect_timer(s_operator, s_timer);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to connect operator to timer: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Comparator ------------------------------------------------------ */
    mcpwm_comparator_config_t cmp_cfg = {
        .flags.update_cmp_on_tez = true,    /* update on timer == zero */
    };
    err = mcpwm_new_comparator(s_operator, &cmp_cfg, &s_comparator);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to create comparator: %s", esp_err_to_name(err));
        return err;
    }

    /* Start at centre (90°) */
    uint32_t initial_pulse = angle_to_pulse_us(90.0f);
    err = mcpwm_comparator_set_compare_value(s_comparator, initial_pulse);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to set initial compare value: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Generator ------------------------------------------------------- */
    /* Explicitly drive the GPIO as output before handing it to MCPWM.
     * Without this the pad may be Hi-Z and the PWM signal never appears. */
    gpio_config_t servo_gpio_cfg = {
        .pin_bit_mask   = (1ULL << SERVO_GPIO),
        .mode           = GPIO_MODE_OUTPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&servo_gpio_cfg);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to configure GPIO %d: %s", SERVO_GPIO, esp_err_to_name(err));
        return err;
    }
    gpio_set_level(SERVO_GPIO, 0);  /* start LOW before MCPWM takes over */

    mcpwm_generator_config_t gen_cfg = {
        .gen_gpio_num = SERVO_GPIO,
    };
    err = mcpwm_new_generator(s_operator, &gen_cfg, &s_generator);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to create generator on GPIO %d: %s", SERVO_GPIO, esp_err_to_name(err));
        return err;
    }

    /* Set high at timer == zero, set low at comparator match */
    err = mcpwm_generator_set_action_on_timer_event(
        s_generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                     MCPWM_TIMER_EVENT_EMPTY,
                                     MCPWM_GEN_ACTION_HIGH));
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to set generator HIGH action: %s", esp_err_to_name(err));
        return err;
    }

    err = mcpwm_generator_set_action_on_compare_event(
        s_generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                        s_comparator,
                                        MCPWM_GEN_ACTION_LOW));
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to set generator LOW action: %s", esp_err_to_name(err));
        return err;
    }

    /* --- Start timer ----------------------------------------------------- */
    err = mcpwm_timer_enable(s_timer);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to enable MCPWM timer: %s", esp_err_to_name(err));
        return err;
    }

    err = mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to start MCPWM timer: %s", esp_err_to_name(err));
        return err;
    }

    SERVO_INFO("Servo initialised on GPIO %d at 90°", SERVO_GPIO);
    return ESP_OK;
}

esp_err_t servo_set_angle(float angle_deg)
{
    if (angle_deg < SERVO_MIN_ANGLE || angle_deg > SERVO_MAX_ANGLE)
        return ESP_ERR_INVALID_ARG;

    uint32_t pulse_us = angle_to_pulse_us(angle_deg);
    SERVO_INFO("Set angle %.1f° → pulse %lu µs", angle_deg, (unsigned long)pulse_us);
    esp_err_t err = servo_set_pulse_us(pulse_us);
    if (err == ESP_OK)
        s_current_angle = angle_deg;
    else
        SERVO_ERROR("servo_set_pulse_us failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t servo_set_pulse_us(uint32_t pulse_us)
{
    if (s_comparator == NULL)
        return ESP_ERR_INVALID_STATE;

    return mcpwm_comparator_set_compare_value(s_comparator, pulse_us);
}

/* -------------------------------------------------------------------------
 * Jog task: smoothly moves servo from current angle to target over duration
 * ------------------------------------------------------------------------- */
static void servo_jog_task(void *arg)
{
    jog_args_t *a        = (jog_args_t *)arg;
    float       to       = a->target;
    uint32_t    dur_ms   = a->duration_ms;
    free(a);

    const uint32_t step_ms = 20;                      /* 50 Hz update rate  */
    uint32_t steps = dur_ms / step_ms;
    if (steps == 0) steps = 1;

    float from  = s_current_angle;
    float delta = (to - from) / (float)steps;

    for (uint32_t i = 0; i < steps; i++) {
        if (s_jog_cancel) break;
        float angle = from + delta * (float)(i + 1);
        if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
        if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
        mcpwm_comparator_set_compare_value(s_comparator, angle_to_pulse_us(angle));
        s_current_angle = angle;
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }

    if (!s_jog_cancel) {
        mcpwm_comparator_set_compare_value(s_comparator, angle_to_pulse_us(to));
        s_current_angle = to;
        SERVO_INFO("Jog complete at %.1f°", to);
    }

    s_jog_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t servo_jog_to_angle(float target_deg, uint32_t duration_ms)
{
    if (target_deg < SERVO_MIN_ANGLE || target_deg > SERVO_MAX_ANGLE)
        return ESP_ERR_INVALID_ARG;
    if (s_comparator == NULL)
        return ESP_ERR_INVALID_STATE;

    /* Cancel any running jog and wait for it to exit */
    if (s_jog_task != NULL) {
        s_jog_cancel = true;
        while (s_jog_task != NULL) {
            vTaskDelay(pdMS_TO_TICKS(25));
        }
    }
    s_jog_cancel = false;

    jog_args_t *a = malloc(sizeof(jog_args_t));
    if (a == NULL) return ESP_ERR_NO_MEM;
    a->target      = target_deg;
    a->duration_ms = duration_ms;

    BaseType_t ret = xTaskCreate(servo_jog_task, "servo_jog", 4096, a, 5, &s_jog_task);
    if (ret != pdPASS) {
        free(a);
        return ESP_FAIL;
    }

    SERVO_INFO("Jogging %.1f° → %.1f° over %lu ms", s_current_angle, target_deg, (unsigned long)duration_ms);
    return ESP_OK;
}

void servo_deinit(void)
{
    if (s_timer)      { mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_STOP_EMPTY); mcpwm_timer_disable(s_timer); }
    if (s_generator)  { mcpwm_del_generator(s_generator);   s_generator  = NULL; }
    if (s_comparator) { mcpwm_del_comparator(s_comparator); s_comparator = NULL; }
    if (s_operator)   { mcpwm_del_operator(s_operator);     s_operator   = NULL; }
    if (s_timer)      { mcpwm_del_timer(s_timer);           s_timer      = NULL; }

    SERVO_INFO("Servo deinitialized");
}
