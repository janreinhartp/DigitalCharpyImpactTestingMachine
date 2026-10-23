#ifndef _WIFI_PORTAL_H_
#define _WIFI_PORTAL_H_

#include "esp_err.h"
#include <stdbool.h>

/* SoftAP the laptop joins to download CSV files. */
#define WIFI_PORTAL_SSID "Charpy-Impact"
#define WIFI_PORTAL_PASS "charpy-data"
#define WIFI_PORTAL_URL  "http://192.168.4.1"

/**
 * @brief Start the WPA2 hotspot.
 *
 * Requires NVS to be initialized. Failure leaves the rest of the machine
 * running; the ESP32-C6 must already have matching ESP-Hosted slave firmware.
 * A failed start tears the driver back down so a later attempt can retry.
 *
 * @return ESP_OK when the hotspot is up
 */
esp_err_t wifi_portal_start(void);

/**
 * @brief Stop the hotspot and release the ESP-Hosted SDIO driver.
 *
 * Safe to call when the hotspot is already down.
 */
esp_err_t wifi_portal_stop(void);

/**
 * @brief True after wifi_portal_start() has brought the hotspot up.
 */
bool wifi_portal_is_started(void);

#endif
