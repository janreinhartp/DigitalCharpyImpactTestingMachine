#ifndef _BSP_PCF8575_H_
#define _BSP_PCF8575_H_

#include "esp_log.h"
#include "esp_err.h"
#include "bsp_i2c.h"

#define PCF8575_TAG "PCF8575"
#define PCF8575_INFO(fmt, ...)  ESP_LOGI(PCF8575_TAG, fmt, ##__VA_ARGS__)
#define PCF8575_DEBUG(fmt, ...) ESP_LOGD(PCF8575_TAG, fmt, ##__VA_ARGS__)
#define PCF8575_ERROR(fmt, ...) ESP_LOGE(PCF8575_TAG, fmt, ##__VA_ARGS__)

/* I2C address — A0=A1=A2=GND → 0x20 */
#define PCF8575_I2C_ADDR    0x20

/* Pin numbers P0–P15 (P0–P7 = low byte, P8–P15 = high byte) */
#define PCF8575_P0          0
#define PCF8575_P1          1
#define PCF8575_P2          2
#define PCF8575_P3          3
#define PCF8575_P4          4
#define PCF8575_P5          5
#define PCF8575_P6          6
#define PCF8575_P7          7
#define PCF8575_P8          8
#define PCF8575_P9          9
#define PCF8575_P10         10
#define PCF8575_P11         11
#define PCF8575_P12         12
#define PCF8575_P13         13
#define PCF8575_P14         14
#define PCF8575_P15         15

/**
 * @brief Initialise the PCF8575 — registers the I2C device and sets all
 *        16 pins high (quasi-bidirectional inputs / outputs-off).
 * @return ESP_OK on success
 */
esp_err_t pcf8575_init(void);

/**
 * @brief Write all 16 pins at once.
 * @param port_val  Bit mask: bit n corresponds to pin Pn. 1 = high, 0 = low.
 * @return ESP_OK on success
 */
esp_err_t pcf8575_write_port(uint16_t port_val);

/**
 * @brief Read all 16 pins at once.
 * @param[out] port_val  Receives the current 16-bit pin state.
 * @return ESP_OK on success
 */
esp_err_t pcf8575_read_port(uint16_t *port_val);

/**
 * @brief Drive a single pin high (output off / input mode).
 * @param pin  Pin number 0–15
 * @return ESP_OK on success
 */
esp_err_t pcf8575_pin_set(uint8_t pin);

/**
 * @brief Drive a single pin low (sinks current, activates active-low loads).
 * @param pin  Pin number 0–15
 * @return ESP_OK on success
 */
esp_err_t pcf8575_pin_clear(uint8_t pin);

/**
 * @brief Read the current logic state of a single pin from the PCF8575.
 * @param pin    Pin number 0–15
 * @param[out] state  true = high, false = low
 * @return ESP_OK on success
 */
esp_err_t pcf8575_pin_get(uint8_t pin, bool *state);

#endif /* _BSP_PCF8575_H_ */
