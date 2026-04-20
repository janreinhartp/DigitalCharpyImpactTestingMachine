#include "bsp_rtc.h"
#include <stdio.h>
#include <string.h>

static i2c_master_dev_handle_t ds3231_dev = NULL;

/* BCD conversion helpers */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

esp_err_t rtc_init(void)
{
    ds3231_dev = i2c_dev_register(DS3231_I2C_ADDR);
    if (ds3231_dev == NULL) {
        RTC_ERROR("Failed to register DS3231 on I2C bus");
        return ESP_FAIL;
    }

    /* Read status register to verify communication */
    uint8_t status;
    esp_err_t err = i2c_read_reg(ds3231_dev, DS3231_REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        RTC_ERROR("Failed to read DS3231 status register");
        return err;
    }

    /* Clear oscillator stop flag if set (bit 7) */
    if (status & 0x80) {
        RTC_INFO("Oscillator was stopped, clearing OSF flag");
        status &= ~0x80;
        err = i2c_write_reg(ds3231_dev, DS3231_REG_STATUS, status);
        if (err != ESP_OK)
            return err;
    }

    /* Disable square wave output, enable battery-backed oscillator */
    err = i2c_write_reg(ds3231_dev, DS3231_REG_CONTROL, 0x04);
    if (err != ESP_OK)
        return err;

    RTC_INFO("DS3231 initialized successfully");
    return ESP_OK;
}

esp_err_t rtc_get_datetime(rtc_datetime_t *dt)
{
    if (dt == NULL || ds3231_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t buf[7];
    esp_err_t err = i2c_read_reg(ds3231_dev, DS3231_REG_SECONDS, buf, 7);
    if (err != ESP_OK)
        return err;

    dt->seconds     = bcd_to_dec(buf[0] & 0x7F);
    dt->minutes     = bcd_to_dec(buf[1] & 0x7F);
    dt->hours       = bcd_to_dec(buf[2] & 0x3F);  /* 24-hour mode */
    dt->day_of_week = buf[3] & 0x07;
    dt->date        = bcd_to_dec(buf[4] & 0x3F);
    dt->month       = bcd_to_dec(buf[5] & 0x1F);
    dt->year        = 2000 + bcd_to_dec(buf[6]);

    return ESP_OK;
}

esp_err_t rtc_set_datetime(const rtc_datetime_t *dt)
{
    if (dt == NULL || ds3231_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t buf[8];
    buf[0] = DS3231_REG_SECONDS;
    buf[1] = dec_to_bcd(dt->seconds);
    buf[2] = dec_to_bcd(dt->minutes);
    buf[3] = dec_to_bcd(dt->hours);     /* 24-hour mode */
    buf[4] = dt->day_of_week & 0x07;
    buf[5] = dec_to_bcd(dt->date);
    buf[6] = dec_to_bcd(dt->month);
    buf[7] = dec_to_bcd((uint8_t)(dt->year - 2000));

    return i2c_write(ds3231_dev, buf, 8);
}

void rtc_format_time(const rtc_datetime_t *dt, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "%02u:%02u:%02u", dt->hours, dt->minutes, dt->seconds);
}

void rtc_format_date(const rtc_datetime_t *dt, char *buf, size_t buf_size)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    uint8_t m = dt->month;
    if (m < 1 || m > 12) m = 1;
    snprintf(buf, buf_size, "%02u-%s-%04u", dt->date, months[m - 1], dt->year);
}

void rtc_format_timestamp(const rtc_datetime_t *dt, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "%04u-%02u-%02u %02u:%02u:%02u",
             dt->year, dt->month, dt->date,
             dt->hours, dt->minutes, dt->seconds);
}

void rtc_format_date_compact(const rtc_datetime_t *dt, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "%04u%02u%02u", dt->year, dt->month, dt->date);
}

esp_err_t rtc_read_temperature(float *temp_c)
{
    if (temp_c == NULL || ds3231_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t buf[2];
    esp_err_t err = i2c_read_reg(ds3231_dev, DS3231_REG_TEMP_H, buf, 2);
    if (err != ESP_OK)
        return err;

    /* Temperature: integer in buf[0], fraction (0.25°C steps) in upper 2 bits of buf[1] */
    int8_t integer_part = (int8_t)buf[0];
    uint8_t frac_part = (buf[1] >> 6) & 0x03;
    *temp_c = (float)integer_part + (float)frac_part * 0.25f;

    return ESP_OK;
}
