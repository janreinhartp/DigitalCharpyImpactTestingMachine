#ifndef _WEB_EXPORT_H_
#define _WEB_EXPORT_H_

#include "esp_err.h"

/**
 * @brief Serve the CSV download page on the hotspot.
 *
 * Call after wifi_portal_start() succeeds. GET / lists CHARPY_*.csv and
 * DBTT_results.csv. GET /download?name= streams one of those files.
 *
 * @return ESP_OK when the server is listening
 */
esp_err_t web_export_start(void);

/**
 * @brief Stop the CSV download server.
 *
 * Safe to call when the server is already stopped.
 */
esp_err_t web_export_stop(void);

#endif
