#ifndef _BSP_ANGLE_H_
#define _BSP_ANGLE_H_

#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define ANGLE_TAG "ANGLE"
#define ANGLE_INFO(fmt, ...) ESP_LOGI(ANGLE_TAG, fmt, ##__VA_ARGS__)
#define ANGLE_DEBUG(fmt, ...) ESP_LOGD(ANGLE_TAG, fmt, ##__VA_ARGS__)
#define ANGLE_ERROR(fmt, ...) ESP_LOGE(ANGLE_TAG, fmt, ##__VA_ARGS__)

/* ── Potentiometer wiring ────────────────────────────────────────────────
 *  Pin 1 (left / CW end)  → 3.3 V
 *  Pin 2 (wiper)          → GPIO 16  (ADC1_CH0)
 *  Pin 3 (right / CCW end)→ GND
 * ─────────────────────────────────────────────────────────────────────── */
#define POT_GPIO          49
#define POT_ADC_UNIT      ADC_UNIT_2
#define POT_ADC_CHANNEL   ADC_CHANNEL_0   /* GPIO 49 = ADC2_CH0 */
#define POT_ADC_ATTEN     ADC_ATTEN_DB_12 /* 0 – ~3.1 V input range */



/**
 * @brief Initialize AS5600 angle sensor on the shared I2C bus
 * @return ESP_OK on success
 */
esp_err_t angle_init(void);

/**
 * @brief Read raw 12-bit angle value (0-4095)
 * @param[out] raw_angle Pointer to store the raw angle
 * @return ESP_OK on success
 */
esp_err_t angle_read_raw(uint16_t *raw_angle);

/**
 * @brief Read angle in degrees (0.0–360.0), with zero offset and pulley
 *        scale factor applied.
 * @param[out] degrees Pointer to store the arm angle in degrees
 * @return ESP_OK on success
 */
esp_err_t angle_read_degrees(float *degrees);

/**
 * @brief Set the zero offset (raw 12-bit units; stored in sensor domain)
 * @param offset Raw offset value (0-4095)
 */
void angle_set_zero_offset(uint16_t offset);

/**
 * @brief Get the current zero offset (raw 12-bit units)
 */
uint16_t angle_get_zero_offset(void);

/**
 * @brief Snapshot current raw reading as zero reference (single-point shortcut)
 * @return ESP_OK on success
 */
esp_err_t angle_calibrate_zero(void);

/* ── Pulley scale factor ─────────────────────────────────────────────── */

/**
 * @brief Set the sensor-to-arm scale factor (arm_deg = sensor_deg * scale).
 *        A negative value means the sensor rotates opposite to the arm —
 *        the direction flag is stored internally; the stored scale is always positive.
 * @param scale  Pulley ratio (may be negative to indicate reversed direction)
 */
void  angle_set_scale(float scale);

/**
 * @brief Get the magnitude of the scale factor (always positive)
 */
float angle_get_scale(void);

/**
 * @brief Get the signed scale factor (negative if sensor direction is reversed)
 */
float angle_get_signed_scale(void);

/* ── 3-point calibration ─────────────────────────────────────────────── */

/**
 * @brief Step 1 — Capture home (0°) raw reading
 */
esp_err_t angle_cal_capture_zero(void);

/**
 * @brief Step 2 — Capture arm position and supply the known arm angle
 * @param arm_deg  Known arm angle at this position (degrees, may be negative)
 */
esp_err_t angle_cal_capture_arm(float arm_deg);

/**
 * @brief Step 3 — Capture max-swing position and supply the known arm angle
 * @param arm_deg  Known arm angle at this position (degrees, may be negative)
 */
esp_err_t angle_cal_capture_max(float arm_deg);

/**
 * @brief Compute scale factor from captured points and apply immediately.
 *        Also sets zero_offset from the captured home raw reading.
 * @param[out] scale_out  Resulting scale factor (may be NULL)
 * @return ESP_OK, or ESP_ERR_INVALID_STATE if not all points were captured,
 *         or ESP_ERR_INVALID_ARG if computed scale is out of range.
 */
esp_err_t angle_cal_apply(float *scale_out);

/**
 * @brief Returns true when all three calibration points have been captured
 */
bool angle_cal_is_complete(void);

/**
 * @brief Return the raw sensor value captured in Step 1 (capture zero).
 *        Valid only after angle_cal_capture_zero() has succeeded.
 */
uint16_t angle_cal_get_raw_zero(void);

/**
 * @brief Read magnet status
 * @param[out] detected true if magnet is detected
 * @param[out] too_strong true if magnet is too strong (optional, can be NULL)
 * @param[out] too_weak true if magnet is too weak (optional, can be NULL)
 * @return ESP_OK on success
 */
esp_err_t angle_get_magnet_status(bool *detected, bool *too_strong, bool *too_weak);

/**
 * @brief Read AGC (Automatic Gain Control) value
 * @param[out] agc AGC value (0-255)
 * @return ESP_OK on success
 */
esp_err_t angle_read_agc(uint8_t *agc);

#endif
