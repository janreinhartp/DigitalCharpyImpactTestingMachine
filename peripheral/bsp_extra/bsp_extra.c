/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "bsp_extra.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

static bool motor_relay_state = false;
static bool actuator_relay_state = false;
static endstop_callback_t endstop_cb = NULL;
static TimerHandle_t endstop_debounce_timer = NULL;
static volatile bool endstop_pending = false;

/* Debounce timer callback — fires 50ms after last edge */
static void endstop_debounce_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    bool pressed = (gpio_get_level(GPIO_ENDSTOP) == 0);
    if (endstop_cb != NULL) {
        endstop_cb(pressed);
    }
}

/* GPIO ISR for endstop — resets debounce timer */
static void IRAM_ATTR endstop_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (endstop_debounce_timer != NULL) {
        xTimerResetFromISR(endstop_debounce_timer, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t gpio_extra_init(void)
{
    esp_err_t err = ESP_OK;
    const gpio_config_t gpio_cofig = {
        .pin_bit_mask = (1ULL << GPIO_ACTUATOR_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&gpio_cofig);
    return err;
}

esp_err_t gpio_extra_set_level(bool level)
{
    gpio_set_level(GPIO_ACTUATOR_RELAY, level);
    return ESP_OK;
}

esp_err_t relay_init(void)
{
    esp_err_t err = ESP_OK;

    /* Configure motor relay (IO47) and actuator relay (IO48) as outputs */
    const gpio_config_t relay_cfg = {
        .pin_bit_mask = (1ULL << GPIO_MOTOR_RELAY) | (1ULL << GPIO_ACTUATOR_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&relay_cfg);
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to configure relay GPIOs");
        return err;
    }

    /* Ensure relays start OFF */
    gpio_set_level(GPIO_MOTOR_RELAY, 0);
    gpio_set_level(GPIO_ACTUATOR_RELAY, 0);
    motor_relay_state = false;
    actuator_relay_state = false;

    EXTRA_INFO("Relay GPIOs initialized (motor=IO%d, actuator=IO%d)", GPIO_MOTOR_RELAY, GPIO_ACTUATOR_RELAY);
    return ESP_OK;
}

esp_err_t motor_relay_set(bool on)
{
    gpio_set_level(GPIO_MOTOR_RELAY, on ? 1 : 0);
    motor_relay_state = on;
    EXTRA_DEBUG("Motor relay %s", on ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t actuator_relay_set(bool on)
{
    gpio_set_level(GPIO_ACTUATOR_RELAY, on ? 1 : 0);
    actuator_relay_state = on;
    EXTRA_DEBUG("Actuator relay %s", on ? "ON" : "OFF");
    return ESP_OK;
}

bool motor_relay_get(void)
{
    return motor_relay_state;
}

bool actuator_relay_get(void)
{
    return actuator_relay_state;
}

esp_err_t endstop_init(void)
{
    esp_err_t err = ESP_OK;

    /* Configure endstop switch (IO33) as input with pull-up */
    const gpio_config_t endstop_cfg = {
        .pin_bit_mask = (1ULL << GPIO_ENDSTOP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = true,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    err = gpio_config(&endstop_cfg);
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to configure endstop GPIO");
        return err;
    }

    /* Create debounce timer (50ms one-shot) */
    endstop_debounce_timer = xTimerCreate(
        "endstop_dbnc", pdMS_TO_TICKS(50), pdFALSE, NULL, endstop_debounce_cb);
    if (endstop_debounce_timer == NULL) {
        EXTRA_ERROR("Failed to create endstop debounce timer");
        return ESP_ERR_NO_MEM;
    }

    /* Install GPIO ISR service and add handler */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means ISR service already installed — that's fine */
        EXTRA_ERROR("Failed to install GPIO ISR service");
        return err;
    }
    err = gpio_isr_handler_add(GPIO_ENDSTOP, endstop_isr_handler, NULL);
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to add endstop ISR handler");
        return err;
    }

    EXTRA_INFO("Endstop switch initialized on IO%d (pull-up, debounced)", GPIO_ENDSTOP);
    return ESP_OK;
}

bool endstop_is_pressed(void)
{
    return (gpio_get_level(GPIO_ENDSTOP) == 0);
}

esp_err_t endstop_register_callback(endstop_callback_t cb)
{
    endstop_cb = cb;
    return ESP_OK;
}
