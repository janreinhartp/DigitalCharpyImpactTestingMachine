#ifndef _BSP_EXTRA_H_
#define _BSP_EXTRA_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/

#include <string.h>      // Standard C library for string handling functions
#include <stdint.h>      // Standard C library for fixed-width integer types
#include "esp_log.h"     // ESP-IDF logging library for debug/info/error logs
#include "esp_err.h"     // ESP-IDF error code definitions and handling utilities
#include "bsp_pcf8575.h" // PCF8575 16-bit I2C I/O expander driver

/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define EXTRA_TAG "EXTRA"                           // Define log tag name "EXTRA" used for identifying log messages
#define EXTRA_INFO(fmt, ...) ESP_LOGI(EXTRA_TAG, fmt, ##__VA_ARGS__)   // Macro for info-level logging with tag "EXTRA"
#define EXTRA_DEBUG(fmt, ...) ESP_LOGD(EXTRA_TAG, fmt, ##__VA_ARGS__)  // Macro for debug-level logging with tag "EXTRA"
#define EXTRA_ERROR(fmt, ...) ESP_LOGE(EXTRA_TAG, fmt, ##__VA_ARGS__)  // Macro for error-level logging with tag "EXTRA"

/* PCF8575 pin assignments */
#define PCF8575_PIN_MOTOR_FWD    PCF8575_P0  /* Motor forward   (active-low output) */
#define PCF8575_PIN_MOTOR_REV    PCF8575_P1  /* Motor reverse   (active-low output) */
#define PCF8575_PIN_SAFETY_LOCK  PCF8575_P2  /* Safety lock     (active-low output) */
#define PCF8575_PIN_RELEASE      PCF8575_P3  /* Release latch   (active-low output) */
/* P4–P15 reserved for future expansion */

/* Endstop GPIO pins (direct ESP32 GPIOs, active-low with internal pull-up) */
#define ENDSTOP_TOP_GPIO    50  /* Top endstop    — arm at armed/raised position */
#define ENDSTOP_BOT_GPIO    51  /* Bottom endstop — arm at home/lowered position */

/* Endstop callback type */
typedef void (*endstop_callback_t)(bool pressed);

/* Legacy LED/GPIO init (GPIO48) */
esp_err_t gpio_extra_init(void);
esp_err_t gpio_extra_set_level(bool level);

/* Relay / output control */
esp_err_t relay_init(void);
esp_err_t motor_forward_set(bool on);
esp_err_t motor_reverse_set(bool on);
esp_err_t release_set(bool on);
esp_err_t safety_lock_set(bool on);
bool motor_forward_get(void);
bool motor_reverse_get(void);
bool release_get(void);
bool safety_lock_get(void);

/* Endstop switches */
esp_err_t endstop_init(void);
bool endstop_top_is_pressed(void);
bool endstop_bot_is_pressed(void);
bool endstop_is_pressed(void);          /* alias for endstop_top_is_pressed() */
esp_err_t endstop_register_callback(endstop_callback_t cb);
esp_err_t endstop_bot_register_callback(endstop_callback_t cb);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif