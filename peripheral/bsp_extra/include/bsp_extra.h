#ifndef _BSP_EXTRA_H_
#define _BSP_EXTRA_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/

#include <string.h>      // Standard C library for string handling functions
#include <stdint.h>      // Standard C library for fixed-width integer types
#include "esp_log.h"     // ESP-IDF logging library for debug/info/error logs
#include "esp_err.h"     // ESP-IDF error code definitions and handling utilities
#include "driver/gpio.h" // ESP-IDF GPIO driver for configuring and controlling pins

/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define EXTRA_TAG "EXTRA"                           // Define log tag name "EXTRA" used for identifying log messages
#define EXTRA_INFO(fmt, ...) ESP_LOGI(EXTRA_TAG, fmt, ##__VA_ARGS__)   // Macro for info-level logging with tag "EXTRA"
#define EXTRA_DEBUG(fmt, ...) ESP_LOGD(EXTRA_TAG, fmt, ##__VA_ARGS__)  // Macro for debug-level logging with tag "EXTRA"
#define EXTRA_ERROR(fmt, ...) ESP_LOGE(EXTRA_TAG, fmt, ##__VA_ARGS__)  // Macro for error-level logging with tag "EXTRA"

/* GPIO pin assignments */
#define GPIO_MOTOR_RELAY    47   /* Motor relay output */
#define GPIO_ACTUATOR_RELAY 48   /* Linear actuator relay output (shared with LED) */
#define GPIO_ENDSTOP        33   /* Endstop limit switch input (active low, internal pull-up) */

/* Endstop callback type */
typedef void (*endstop_callback_t)(bool pressed);

/* Legacy LED/GPIO init (GPIO48) */
esp_err_t gpio_extra_init(void);
esp_err_t gpio_extra_set_level(bool level);

/* Relay control */
esp_err_t relay_init(void);
esp_err_t motor_relay_set(bool on);
esp_err_t actuator_relay_set(bool on);
bool motor_relay_get(void);
bool actuator_relay_get(void);

/* Endstop switch */
esp_err_t endstop_init(void);
bool endstop_is_pressed(void);
esp_err_t endstop_register_callback(endstop_callback_t cb);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif