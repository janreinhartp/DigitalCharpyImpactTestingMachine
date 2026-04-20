#ifndef _BSP_RTC_H_
#define _BSP_RTC_H_

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "bsp_i2c.h"

#define RTC_TAG "RTC"
#define RTC_INFO(fmt, ...) ESP_LOGI(RTC_TAG, fmt, ##__VA_ARGS__)
#define RTC_DEBUG(fmt, ...) ESP_LOGD(RTC_TAG, fmt, ##__VA_ARGS__)
#define RTC_ERROR(fmt, ...) ESP_LOGE(RTC_TAG, fmt, ##__VA_ARGS__)

/* DS3231 I2C address */
#define DS3231_I2C_ADDR     0x68

/* DS3231 registers */
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_MINUTES  0x01
#define DS3231_REG_HOURS    0x02
#define DS3231_REG_DAY      0x03
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_TEMP_H   0x11
#define DS3231_REG_TEMP_L   0x12

/**
 * @brief RTC date/time structure
 */
typedef struct {
    uint8_t seconds;    /* 0-59 */
    uint8_t minutes;    /* 0-59 */
    uint8_t hours;      /* 0-23 */
    uint8_t day_of_week;/* 1-7 (1=Monday) */
    uint8_t date;       /* 1-31 */
    uint8_t month;      /* 1-12 */
    uint16_t year;      /* 2000-2099 */
} rtc_datetime_t;

/**
 * @brief Initialize DS3231 RTC on the shared I2C bus
 * @return ESP_OK on success
 */
esp_err_t rtc_init(void);

/**
 * @brief Get current date/time from RTC
 * @param[out] dt Pointer to datetime structure
 * @return ESP_OK on success
 */
esp_err_t rtc_get_datetime(rtc_datetime_t *dt);

/**
 * @brief Set date/time on RTC
 * @param[in] dt Pointer to datetime structure
 * @return ESP_OK on success
 */
esp_err_t rtc_set_datetime(const rtc_datetime_t *dt);

/**
 * @brief Format datetime as "HH:MM:SS" string
 * @param[in] dt Datetime to format
 * @param[out] buf Output buffer (at least 9 bytes)
 * @param[in] buf_size Buffer size
 */
void rtc_format_time(const rtc_datetime_t *dt, char *buf, size_t buf_size);

/**
 * @brief Format datetime as "DD-MMM-YYYY" string
 * @param[in] dt Datetime to format
 * @param[out] buf Output buffer (at least 12 bytes)
 * @param[in] buf_size Buffer size
 */
void rtc_format_date(const rtc_datetime_t *dt, char *buf, size_t buf_size);

/**
 * @brief Format datetime as "YYYY-MM-DD HH:MM:SS" for CSV logging
 * @param[in] dt Datetime to format
 * @param[out] buf Output buffer (at least 20 bytes)
 * @param[in] buf_size Buffer size
 */
void rtc_format_timestamp(const rtc_datetime_t *dt, char *buf, size_t buf_size);

/**
 * @brief Format date as "YYYYMMDD" for filename construction
 * @param[in] dt Datetime to format
 * @param[out] buf Output buffer (at least 9 bytes)
 * @param[in] buf_size Buffer size
 */
void rtc_format_date_compact(const rtc_datetime_t *dt, char *buf, size_t buf_size);

/**
 * @brief Read DS3231 on-chip temperature sensor
 * @param[out] temp_c Temperature in degrees Celsius (resolution 0.25°C)
 * @return ESP_OK on success
 */
esp_err_t rtc_read_temperature(float *temp_c);

#endif
