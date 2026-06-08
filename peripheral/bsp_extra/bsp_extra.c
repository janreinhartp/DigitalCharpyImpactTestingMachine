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

/* Debounce timer callback — reads PCF8575 P2 to get actual endstop state */
static void endstop_debounce_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    bool pin_state = true; /* default high = not pressed */
    pcf8575_pin_get(PCF8575_PIN_ENDSTOP, &pin_state);
    bool pressed = !pin_state; /* active-low */
    if (endstop_cb != NULL) {
        endstop_cb(pressed);
    }
}

/* GPIO ISR for PCF8575 ~INT pin — resets debounce timer */
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
    /* Legacy stub — PCF8575 is initialised by relay_init() */
    return ESP_OK;
}

esp_err_t gpio_extra_set_level(bool level)
{
    /* Maps legacy call to PCF8575 actuator relay pin */
    if (level) {
        return pcf8575_pin_set(PCF8575_PIN_ACTUATOR_RELAY);
    } else {
        return pcf8575_pin_clear(PCF8575_PIN_ACTUATOR_RELAY);
    }
}

esp_err_t relay_init(void)
{
    /* Initialise PCF8575 — sets all pins high (relays OFF, endstop as input) */
    esp_err_t err = pcf8575_init();
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to initialise PCF8575: %s", esp_err_to_name(err));
        return err;
    }

    motor_relay_state    = false;
    actuator_relay_state = false;

    EXTRA_INFO("Relays initialised via PCF8575 (motor=P%d, actuator=P%d)",
               PCF8575_PIN_MOTOR_RELAY, PCF8575_PIN_ACTUATOR_RELAY);
    return ESP_OK;
}

esp_err_t motor_relay_set(bool on)
{
    esp_err_t err = on ? pcf8575_pin_clear(PCF8575_PIN_MOTOR_RELAY)
                       : pcf8575_pin_set(PCF8575_PIN_MOTOR_RELAY);
    if (err == ESP_OK) {
        motor_relay_state = on;
        EXTRA_DEBUG("Motor relay %s", on ? "ON" : "OFF");
    }
    return err;
}

esp_err_t actuator_relay_set(bool on)
{
    esp_err_t err = on ? pcf8575_pin_clear(PCF8575_PIN_ACTUATOR_RELAY)
                       : pcf8575_pin_set(PCF8575_PIN_ACTUATOR_RELAY);
    if (err == ESP_OK) {
        actuator_relay_state = on;
        EXTRA_DEBUG("Actuator relay %s", on ? "ON" : "OFF");
    }
    return err;
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

    /* PCF8575 P2 is already high (input mode) after relay_init().
     * Configure ESP32 GPIO33 as input connected to PCF8575 ~INT (open-drain, active-low). */
    const gpio_config_t int_cfg = {
        .pin_bit_mask = (1ULL << PCF8575_GPIO_INT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_NEGEDGE, /* INT fires on falling edge */
    };
    err = gpio_config(&int_cfg);
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to configure PCF8575 INT GPIO");
        return err;
    }

    /* Create debounce timer (50ms one-shot) */
    endstop_debounce_timer = xTimerCreate(
        "endstop_dbnc", pdMS_TO_TICKS(50), pdFALSE, NULL, endstop_debounce_cb);
    if (endstop_debounce_timer == NULL) {
        EXTRA_ERROR("Failed to create endstop debounce timer");
        return ESP_ERR_NO_MEM;
    }

    /* Install GPIO ISR service and add handler for PCF8575 INT pin */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        EXTRA_ERROR("Failed to install GPIO ISR service");
        return err;
    }
    err = gpio_isr_handler_add(PCF8575_GPIO_INT, endstop_isr_handler, NULL);
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to add PCF8575 INT ISR handler");
        return err;
    }

    EXTRA_INFO("Endstop initialized via PCF8575 P%d, INT on GPIO%d",
               PCF8575_PIN_ENDSTOP, PCF8575_GPIO_INT);
    return ESP_OK;
}

bool endstop_is_pressed(void)
{
    bool state = true; /* default: not pressed */
    pcf8575_pin_get(PCF8575_PIN_ENDSTOP, &state);
    return !state; /* active-low */
}

esp_err_t endstop_register_callback(endstop_callback_t cb)
{
    endstop_cb = cb;
    return ESP_OK;
}
