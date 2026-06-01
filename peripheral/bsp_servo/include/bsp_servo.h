#ifndef _BSP_SERVO_H_
#define _BSP_SERVO_H_

#include "esp_log.h"
#include "esp_err.h"
#include "driver/mcpwm_prelude.h"

#define SERVO_TAG "SERVO"
#define SERVO_INFO(fmt, ...)  ESP_LOGI(SERVO_TAG, fmt, ##__VA_ARGS__)
#define SERVO_DEBUG(fmt, ...) ESP_LOGD(SERVO_TAG, fmt, ##__VA_ARGS__)
#define SERVO_ERROR(fmt, ...) ESP_LOGE(SERVO_TAG, fmt, ##__VA_ARGS__)

/* GPIO connected to servo signal wire (PWM output, 3.3 V logic).
 * Servo power (VCC) must come from a dedicated 14 V supply.
 * Share GND between the 14 V supply and the ESP32. */
#define SERVO_GPIO          38

/* Standard servo PWM parameters */
#define SERVO_FREQ_HZ       50          /* 50 Hz → 20 ms period               */
#define SERVO_TIMER_RES_HZ  1000000     /* 1 MHz resolution → 1 µs per tick   */
#define SERVO_PERIOD_TICKS  20000       /* 20 ms period in ticks               */

/* Pulse-width limits — adjust to match your specific servo */
#define SERVO_MIN_PULSE_US  500         /* µs at 0°   */
#define SERVO_MAX_PULSE_US  2500        /* µs at 180° */
#define SERVO_MIN_ANGLE     0.0f        /* degrees    */
#define SERVO_MAX_ANGLE     180.0f      /* degrees    */

/**
 * @brief Initialise the MCPWM timer, operator, comparator and generator
 *        for servo control on SERVO_GPIO.
 * @return ESP_OK on success
 */
esp_err_t servo_init(void);

/**
 * @brief Move the servo to an absolute angle.
 * @param angle_deg  Target angle in degrees [0.0 … 180.0]
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 */
esp_err_t servo_set_angle(float angle_deg);

/**
 * @brief Set servo position by raw pulse width.
 * @param pulse_us  Pulse width in microseconds [SERVO_MIN_PULSE_US … SERVO_MAX_PULSE_US]
 * @return ESP_OK on success
 */
esp_err_t servo_set_pulse_us(uint32_t pulse_us);

/**
 * @brief Stop the MCPWM timer and free all resources.
 */
void servo_deinit(void);

#endif /* _BSP_SERVO_H_ */
