#include "bsp_rtc.h"
#include <stdio.h>
#include <string.h>

static i2c_master_dev_handle_t ds1307_dev = NULL;

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
    ds1307_dev = i2c_dev_register(DS1307_I2C_ADDR);
    if (ds1307_dev == NULL) {
        RTC_ERROR("Failed to register DS1307 on I2C bus");
        return ESP_FAIL;
    }

    /* Read seconds register — bit 7 is the Clock Halt (CH) bit */
    uint8_t seconds;
    esp_err_t err = i2c_read_reg(ds1307_dev, DS1307_REG_SECONDS, &seconds, 1);
    if (err != ESP_OK) {
        RTC_ERROR("Failed to read DS1307 seconds register");
        return err;
    }

    /* Clear CH bit to start the oscillator if it was halted */
    if (seconds & DS1307_CH_BIT) {
        RTC_INFO("DS1307 oscillator was halted, starting it now");
        seconds &= ~DS1307_CH_BIT;
        err = i2c_write_reg(ds1307_dev, DS1307_REG_SECONDS, seconds);
        if (err != ESP_OK)
            return err;
    }

    /* Disable SQW/OUT pin (square wave output off) */
    err = i2c_write_reg(ds1307_dev, DS1307_REG_CONTROL, 0x00);
    if (err != ESP_OK)
        return err;

    RTC_INFO("DS1307 (Tiny RTC) initialized successfully");
    return ESP_OK;
}

esp_err_t rtc_get_datetime(rtc_datetime_t *dt)
{
    if (dt == NULL || ds1307_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t buf[7];
    esp_err_t err = i2c_read_reg(ds1307_dev, DS1307_REG_SECONDS, buf, 7);
    if (err != ESP_OK)
        return err;

    dt->seconds     = bcd_to_dec(buf[0] & 0x7F);  /* mask CH bit */
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
    if (dt == NULL || ds1307_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t buf[8];
    buf[0] = DS1307_REG_SECONDS;          /* CH bit is 0 — oscillator running */
    buf[1] = dec_to_bcd(dt->seconds);
    buf[2] = dec_to_bcd(dt->minutes);
    buf[3] = dec_to_bcd(dt->hours);       /* 24-hour mode, bit 6 = 0 */
    buf[4] = dt->day_of_week & 0x07;
    buf[5] = dec_to_bcd(dt->date);
    buf[6] = dec_to_bcd(dt->month);
    buf[7] = dec_to_bcd((uint8_t)(dt->year - 2000));

    return i2c_write(ds1307_dev, buf, 8);
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
    /* DS1307 (Tiny RTC) has no on-chip temperature sensor */
    (void)temp_c;
    return ESP_ERR_NOT_SUPPORTED;
}
