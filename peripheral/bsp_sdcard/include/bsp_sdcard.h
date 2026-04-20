#ifndef _BSP_SDCARD_H_
#define _BSP_SDCARD_H_

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#define SD_TAG "SDCARD"
#define SD_INFO(fmt, ...) ESP_LOGI(SD_TAG, fmt, ##__VA_ARGS__)
#define SD_DEBUG(fmt, ...) ESP_LOGD(SD_TAG, fmt, ##__VA_ARGS__)
#define SD_ERROR(fmt, ...) ESP_LOGE(SD_TAG, fmt, ##__VA_ARGS__)

/* SDMMC GPIO pins (CrowPanel built-in SD card slot) */
#define SD_GPIO_CMD     44
#define SD_GPIO_CLK     43
#define SD_GPIO_D0      39

#define SD_MOUNT_POINT  "/sdcard"

/**
 * @brief Initialize SD card with SDMMC 1-line mode and mount FAT filesystem
 * @return ESP_OK on success
 */
esp_err_t sdcard_init(void);

/**
 * @brief Check if SD card is mounted
 * @return true if mounted
 */
bool sdcard_is_mounted(void);

/**
 * @brief Append a line of text to a file (creates if not exists)
 * @param filepath Full path (e.g. "/sdcard/CHARPY_20260420.csv")
 * @param line Text to append (newline added automatically)
 * @return ESP_OK on success
 */
esp_err_t sdcard_append_line(const char *filepath, const char *line);

/**
 * @brief Create a file with a header line if it doesn't already exist
 * @param filepath Full path
 * @param header CSV header line
 * @return ESP_OK on success
 */
esp_err_t sdcard_ensure_file_with_header(const char *filepath, const char *header);

/**
 * @brief Read all lines from a file into a callback
 * @param filepath Full path
 * @param line_cb Callback called for each line (receives line string and user data)
 * @param user_data Pointer passed to callback
 * @return ESP_OK on success
 */
esp_err_t sdcard_read_lines(const char *filepath,
                            void (*line_cb)(const char *line, void *user_data),
                            void *user_data);

/**
 * @brief Check if a file exists
 * @param filepath Full path
 * @return true if file exists
 */
bool sdcard_file_exists(const char *filepath);

/**
 * @brief Get SD card free and total space in bytes
 * @param[out] total_bytes Total space (can be NULL)
 * @param[out] free_bytes Free space (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t sdcard_get_space(uint64_t *total_bytes, uint64_t *free_bytes);

#endif
