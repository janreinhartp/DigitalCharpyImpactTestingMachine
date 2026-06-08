#include "bsp_pcf8575.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static i2c_master_dev_handle_t s_dev       = NULL;
static uint16_t                s_shadow    = 0xFFFF; /* all pins high on power-up */
static SemaphoreHandle_t       s_mutex     = NULL;

/* ---- helpers ------------------------------------------------------------ */

static esp_err_t pcf8575_flush(void)
{
    uint8_t buf[2] = {
        (uint8_t)(s_shadow & 0xFF),         /* P0–P7  */
        (uint8_t)((s_shadow >> 8) & 0xFF),  /* P8–P15 */
    };
    return i2c_write(s_dev, buf, sizeof(buf));
}

/* ---- public API --------------------------------------------------------- */

esp_err_t pcf8575_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        PCF8575_ERROR("Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    s_dev = i2c_dev_register(PCF8575_I2C_ADDR);
    if (s_dev == NULL) {
        PCF8575_ERROR("Failed to register PCF8575 on I2C bus");
        return ESP_FAIL;
    }

    /* Drive all pins high (quasi-bidirectional default state) */
    s_shadow = 0xFFFF;
    esp_err_t err = pcf8575_flush();
    if (err != ESP_OK) {
        PCF8575_ERROR("Failed to initialise PCF8575 port: %s", esp_err_to_name(err));
        return err;
    }

    PCF8575_INFO("PCF8575 initialised at 0x%02X", PCF8575_I2C_ADDR);
    return ESP_OK;
}

esp_err_t pcf8575_write_port(uint16_t port_val)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_shadow = port_val;
    esp_err_t err = pcf8575_flush();
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t pcf8575_read_port(uint16_t *port_val)
{
    if (s_dev == NULL || port_val == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t buf[2] = {0, 0};
    esp_err_t err = i2c_read(s_dev, buf, sizeof(buf));
    if (err == ESP_OK) {
        *port_val = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    }
    return err;
}

esp_err_t pcf8575_pin_set(uint8_t pin)
{
    if (pin > 15) return ESP_ERR_INVALID_ARG;
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_shadow |= (uint16_t)(1u << pin);
    esp_err_t err = pcf8575_flush();
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t pcf8575_pin_clear(uint8_t pin)
{
    if (pin > 15) return ESP_ERR_INVALID_ARG;
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_shadow &= (uint16_t)~(1u << pin);
    esp_err_t err = pcf8575_flush();
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t pcf8575_pin_get(uint8_t pin, bool *state)
{
    if (pin > 15 || state == NULL) return ESP_ERR_INVALID_ARG;

    uint16_t port_val;
    esp_err_t err = pcf8575_read_port(&port_val);
    if (err == ESP_OK) {
        *state = (port_val >> pin) & 0x01;
    }
    return err;
}
