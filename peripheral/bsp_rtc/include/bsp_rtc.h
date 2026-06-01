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

/* DS1307 (Tiny RTC module) I2C address */
#define DS1307_I2C_ADDR     0x68

/* DS1307 registers */
#define DS1307_REG_SECONDS  0x00  /* bit 7 = CH (Clock Halt) */
#define DS1307_REG_MINUTES  0x01
#define DS1307_REG_HOURS    0x02
#define DS1307_REG_DAY      0x03
#define DS1307_REG_DATE     0x04
#define DS1307_REG_MONTH    0x05
#define DS1307_REG_YEAR     0x06
#define DS1307_REG_CONTROL  0x07  /* SQW/OUT control */
#define DS1307_CH_BIT       0x80  /* Clock Halt bit in seconds register */

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
 * @brief Initialize DS1307 (Tiny RTC) on the shared I2C bus
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
 * @brief Read on-chip temperature (not supported on DS1307 / Tiny RTC)
 * @return ESP_ERR_NOT_SUPPORTED always
 */
esp_err_t rtc_read_temperature(float *temp_c);

#endif
