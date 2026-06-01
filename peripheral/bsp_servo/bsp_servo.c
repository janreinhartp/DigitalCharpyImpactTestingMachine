#include "bsp_servo.h"
#include <stddef.h>

static mcpwm_timer_handle_t    s_timer     = NULL;
static mcpwm_oper_handle_t     s_operator  = NULL;
static mcpwm_cmpr_handle_t     s_comparator = NULL;
static mcpwm_gen_handle_t      s_generator  = NULL;

/* Map an angle in degrees to a pulse width in microseconds */
static inline uint32_t angle_to_pulse_us(float angle_deg)
{
    if (angle_deg < SERVO_MIN_ANGLE) angle_deg = SERVO_MIN_ANGLE;
    if (angle_deg > SERVO_MAX_ANGLE) angle_deg = SERVO_MAX_ANGLE;

    float ratio = (angle_deg - SERVO_MIN_ANGLE) / (SERVO_MAX_ANGLE - SERVO_MIN_ANGLE);
    return (uint32_t)(SERVO_MIN_PULSE_US + ratio * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US));
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
    if (err != ESP_OK) return err;

    /* --- Generator ------------------------------------------------------- */
    mcpwm_generator_config_t gen_cfg = {
        .gen_gpio_num = SERVO_GPIO,
    };
    err = mcpwm_new_generator(s_operator, &gen_cfg, &s_generator);
    if (err != ESP_OK) {
        SERVO_ERROR("Failed to create generator: %s", esp_err_to_name(err));
        return err;
    }

    /* Set high at timer == zero, set low at comparator match */
    err = mcpwm_generator_set_action_on_timer_event(
        s_generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                     MCPWM_TIMER_EVENT_EMPTY,
                                     MCPWM_GEN_ACTION_HIGH));
    if (err != ESP_OK) return err;

    err = mcpwm_generator_set_action_on_compare_event(
        s_generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                        s_comparator,
                                        MCPWM_GEN_ACTION_LOW));
    if (err != ESP_OK) return err;

    /* --- Start timer ----------------------------------------------------- */
    err = mcpwm_timer_enable(s_timer);
    if (err != ESP_OK) return err;

    err = mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP);
    if (err != ESP_OK) return err;

    SERVO_INFO("Servo initialised on GPIO %d at 90°", SERVO_GPIO);
    return ESP_OK;
}

esp_err_t servo_set_angle(float angle_deg)
{
    if (angle_deg < SERVO_MIN_ANGLE || angle_deg > SERVO_MAX_ANGLE)
        return ESP_ERR_INVALID_ARG;

    uint32_t pulse_us = angle_to_pulse_us(angle_deg);
    return servo_set_pulse_us(pulse_us);
}

esp_err_t servo_set_pulse_us(uint32_t pulse_us)
{
    if (s_comparator == NULL)
        return ESP_ERR_INVALID_STATE;

    return mcpwm_comparator_set_compare_value(s_comparator, pulse_us);
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
