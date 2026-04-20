#ifndef _DATA_LOGGER_H_
#define _DATA_LOGGER_H_

#include "esp_err.h"
#include "charpy_calc.h"

/* Maximum number of recent results cached in RAM for UI history */
#define HISTORY_CACHE_SIZE  50

/**
 * @brief Initialize the data logger (requires SD card to be mounted)
 * @return ESP_OK on success
 */
esp_err_t data_logger_init(void);

/**
 * @brief Write a test result to the daily CSV file on SD card
 * @param result Test result to write
 * @return ESP_OK on success
 */
esp_err_t data_logger_write_result(const test_result_t *result);

/**
 * @brief Get the cached history of recent test results
 * @param[out] results Pointer to array of results (do not free)
 * @param[out] count Number of results in array
 * @return ESP_OK on success
 */
esp_err_t data_logger_get_history(const test_result_t **results, int *count);

/**
 * @brief Load history from SD card (called at startup)
 * @return ESP_OK on success
 */
esp_err_t data_logger_load_history(void);

/**
 * @brief Get the total number of test records on SD card
 * @return Record count (approximate, from cached history)
 */
int data_logger_get_record_count(void);

#endif
