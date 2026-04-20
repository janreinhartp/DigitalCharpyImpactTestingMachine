#ifndef _BSP_ANGLE_H_
#define _BSP_ANGLE_H_

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "bsp_i2c.h"

#define ANGLE_TAG "ANGLE"
#define ANGLE_INFO(fmt, ...) ESP_LOGI(ANGLE_TAG, fmt, ##__VA_ARGS__)
#define ANGLE_DEBUG(fmt, ...) ESP_LOGD(ANGLE_TAG, fmt, ##__VA_ARGS__)
#define ANGLE_ERROR(fmt, ...) ESP_LOGE(ANGLE_TAG, fmt, ##__VA_ARGS__)

/* AS5600 I2C address */
#define AS5600_I2C_ADDR     0x36

/* AS5600 registers */
#define AS5600_REG_RAW_ANGLE_H  0x0C
#define AS5600_REG_RAW_ANGLE_L  0x0D
#define AS5600_REG_ANGLE_H      0x0E
#define AS5600_REG_ANGLE_L      0x0F
#define AS5600_REG_STATUS       0x0B
#define AS5600_REG_AGC          0x1A
#define AS5600_REG_MAGNITUDE_H  0x1B
#define AS5600_REG_MAGNITUDE_L  0x1C

/* Status register bit masks */
#define AS5600_STATUS_MH        0x08  /* Magnet too strong */
#define AS5600_STATUS_ML        0x10  /* Magnet too weak */
#define AS5600_STATUS_MD        0x20  /* Magnet detected */

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
 * @brief Read angle in degrees (0.0 - 360.0), with zero offset applied
 * @param[out] degrees Pointer to store the angle in degrees
 * @return ESP_OK on success
 */
esp_err_t angle_read_degrees(float *degrees);

/**
 * @brief Set the zero offset angle (in raw 12-bit units)
 * @param offset Raw offset value (0-4095)
 */
void angle_set_zero_offset(uint16_t offset);

/**
 * @brief Get the current zero offset
 * @return Current zero offset (0-4095)
 */
uint16_t angle_get_zero_offset(void);

/**
 * @brief Snapshot current position as zero reference
 * @return ESP_OK on success
 */
esp_err_t angle_calibrate_zero(void);

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
