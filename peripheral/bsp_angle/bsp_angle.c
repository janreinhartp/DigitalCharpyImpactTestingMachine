#include "bsp_angle.h"

static i2c_master_dev_handle_t as5600_dev = NULL;
static uint16_t zero_offset = 0;

esp_err_t angle_init(void)
{
    as5600_dev = i2c_dev_register(AS5600_I2C_ADDR);
    if (as5600_dev == NULL) {
        ANGLE_ERROR("Failed to register AS5600 on I2C bus");
        return ESP_FAIL;
    }

    /* Verify magnet is detected */
    bool detected = false;
    esp_err_t err = angle_get_magnet_status(&detected, NULL, NULL);
    if (err != ESP_OK) {
        ANGLE_ERROR("Failed to read AS5600 status");
        return err;
    }
    if (!detected) {
        ANGLE_INFO("WARNING: No magnet detected on AS5600");
    } else {
        ANGLE_INFO("AS5600 initialized, magnet detected");
    }

    return ESP_OK;
}

esp_err_t angle_read_raw(uint16_t *raw_angle)
{
    if (raw_angle == NULL || as5600_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t buf[2];
    esp_err_t err = i2c_read_reg(as5600_dev, AS5600_REG_RAW_ANGLE_H, buf, 2);
    if (err != ESP_OK)
        return err;

    *raw_angle = ((uint16_t)(buf[0] & 0x0F) << 8) | buf[1];
    return ESP_OK;
}

esp_err_t angle_read_degrees(float *degrees)
{
    if (degrees == NULL)
        return ESP_ERR_INVALID_ARG;

    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK)
        return err;

    /* Apply zero offset with wraparound */
    int32_t adjusted = (int32_t)raw - (int32_t)zero_offset;
    if (adjusted < 0)
        adjusted += 4096;

    *degrees = (float)adjusted * 360.0f / 4096.0f;
    return ESP_OK;
}

void angle_set_zero_offset(uint16_t offset)
{
    zero_offset = offset & 0x0FFF;
    ANGLE_INFO("Zero offset set to %u (%.1f°)", zero_offset, (float)zero_offset * 360.0f / 4096.0f);
}

uint16_t angle_get_zero_offset(void)
{
    return zero_offset;
}

esp_err_t angle_calibrate_zero(void)
{
    uint16_t raw;
    esp_err_t err = angle_read_raw(&raw);
    if (err != ESP_OK)
        return err;

    angle_set_zero_offset(raw);
    ANGLE_INFO("Calibrated zero at raw=%u", raw);
    return ESP_OK;
}

esp_err_t angle_get_magnet_status(bool *detected, bool *too_strong, bool *too_weak)
{
    if (detected == NULL || as5600_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t status;
    esp_err_t err = i2c_read_reg(as5600_dev, AS5600_REG_STATUS, &status, 1);
    if (err != ESP_OK)
        return err;

    *detected = (status & AS5600_STATUS_MD) != 0;
    if (too_strong != NULL)
        *too_strong = (status & AS5600_STATUS_MH) != 0;
    if (too_weak != NULL)
        *too_weak = (status & AS5600_STATUS_ML) != 0;

    return ESP_OK;
}

esp_err_t angle_read_agc(uint8_t *agc)
{
    if (agc == NULL || as5600_dev == NULL)
        return ESP_ERR_INVALID_ARG;

    return i2c_read_reg(as5600_dev, AS5600_REG_AGC, agc, 1);
}
