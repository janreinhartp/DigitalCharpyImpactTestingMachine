/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "bsp_extra.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

static bool motor_fwd_state   = false;
static bool motor_rev_state   = false;
static bool release_state     = false;
static bool safety_lock_state = false;

static endstop_callback_t endstop_top_cb = NULL;
static endstop_callback_t endstop_bot_cb = NULL;

/* Polling task — samples both endstops every 20 ms; fires callback on state change */
static void endstop_poll_task(void *arg)
{
    (void)arg;
    bool last_top = false;
    bool last_bot = false;
    uint8_t top_cnt = 0;
    uint8_t bot_cnt = 0;

/* Require this many consecutive matching reads (× 20 ms) before accepting */
#define ENDSTOP_DEBOUNCE_COUNT 5   /* 5 × 20 ms = 100 ms */

    for (;;) {
        bool top = endstop_top_is_pressed();
        bool bot = endstop_bot_is_pressed();

        /* Debounce top endstop */
        if (top != last_top) {
            if (++top_cnt >= ENDSTOP_DEBOUNCE_COUNT) {
                last_top = top;
                top_cnt  = 0;
                EXTRA_INFO("Endstop TOP %s (GPIO%d)", top ? "PRESSED" : "released", ENDSTOP_TOP_GPIO);
                if (endstop_top_cb != NULL) endstop_top_cb(top);
            }
        } else {
            top_cnt = 0;
        }

        /* Debounce bottom endstop */
        if (bot != last_bot) {
            if (++bot_cnt >= ENDSTOP_DEBOUNCE_COUNT) {
                last_bot = bot;
                bot_cnt  = 0;
                EXTRA_INFO("Endstop BOT %s (GPIO%d)", bot ? "PRESSED" : "released", ENDSTOP_BOT_GPIO);
                if (endstop_bot_cb != NULL) endstop_bot_cb(bot);
            }
        } else {
            bot_cnt = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t gpio_extra_init(void)
{
    /* Legacy stub — PCF8575 is initialised by relay_init() */
    return ESP_OK;
}

esp_err_t gpio_extra_set_level(bool level)
{
    /* Maps legacy call to PCF8575 release pin */
    if (level) {
        return pcf8575_pin_set(PCF8575_PIN_RELEASE);
    } else {
        return pcf8575_pin_clear(PCF8575_PIN_RELEASE);
    }
}

esp_err_t relay_init(void)
{
    /* Initialise PCF8575 — sets all pins high (all outputs OFF) */
    esp_err_t err = pcf8575_init();
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to initialise PCF8575: %s", esp_err_to_name(err));
        return err;
    }

    motor_fwd_state   = false;
    motor_rev_state   = false;
    release_state     = false;
    safety_lock_state = true;   /* safety lock engaged on startup */

    /* Engage safety lock: relay OFF = pin HIGH (normally-engaged, fail-safe).
     * pcf8575_init() already sets all pins HIGH, so this is redundant but explicit. */
    pcf8575_pin_set(PCF8575_PIN_SAFETY_LOCK);

    EXTRA_INFO("Relays initialised (fwd=P%d, rev=P%d, lock=P%d, release=P%d)",
               PCF8575_PIN_MOTOR_FWD, PCF8575_PIN_MOTOR_REV,
               PCF8575_PIN_SAFETY_LOCK, PCF8575_PIN_RELEASE);
    return ESP_OK;
}

esp_err_t motor_forward_set(bool on)
{
    esp_err_t err = on ? pcf8575_pin_clear(PCF8575_PIN_MOTOR_FWD)
                       : pcf8575_pin_set(PCF8575_PIN_MOTOR_FWD);
    if (err == ESP_OK) {
        motor_fwd_state = on;
        EXTRA_DEBUG("Motor forward %s", on ? "ON" : "OFF");
    }
    return err;
}

esp_err_t motor_reverse_set(bool on)
{
    esp_err_t err = on ? pcf8575_pin_clear(PCF8575_PIN_MOTOR_REV)
                       : pcf8575_pin_set(PCF8575_PIN_MOTOR_REV);
    if (err == ESP_OK) {
        motor_rev_state = on;
        EXTRA_DEBUG("Motor reverse %s", on ? "ON" : "OFF");
    }
    return err;
}

esp_err_t release_set(bool on)
{
    esp_err_t err = on ? pcf8575_pin_clear(PCF8575_PIN_RELEASE)
                       : pcf8575_pin_set(PCF8575_PIN_RELEASE);
    if (err == ESP_OK) {
        release_state = on;
        EXTRA_DEBUG("Release %s", on ? "ON" : "OFF");
    }
    return err;
}

esp_err_t safety_lock_set(bool on)
{
    /* Normally-engaged (fail-safe): engaged = relay OFF = pin HIGH.
     * on=true  → ENGAGE  → relay OFF → pin HIGH (pcf8575_pin_set)
     * on=false → RELEASE → relay ON  → pin LOW  (pcf8575_pin_clear) */
    esp_err_t err = on ? pcf8575_pin_set(PCF8575_PIN_SAFETY_LOCK)
                       : pcf8575_pin_clear(PCF8575_PIN_SAFETY_LOCK);
    if (err == ESP_OK) {
        safety_lock_state = on;
        EXTRA_DEBUG("Safety lock %s", on ? "ENGAGED" : "RELEASED");
    }
    return err;
}

bool motor_forward_get(void)  { return motor_fwd_state; }
bool motor_reverse_get(void)  { return motor_rev_state; }
bool release_get(void)        { return release_state; }
bool safety_lock_get(void)    { return safety_lock_state; }

esp_err_t endstop_init(void)
{
    /* Configure GPIO 50 and 51 as inputs with internal pull-ups (active-low) */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ENDSTOP_TOP_GPIO) | (1ULL << ENDSTOP_BOT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        EXTRA_ERROR("Failed to configure endstop GPIOs: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t ret = xTaskCreate(endstop_poll_task, "endstop_poll",
                                 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        EXTRA_ERROR("Failed to create endstop poll task");
        return ESP_ERR_NO_MEM;
    }
    EXTRA_INFO("Endstop polling started (top=GPIO%d, bot=GPIO%d, 20 ms interval)",
               ENDSTOP_TOP_GPIO, ENDSTOP_BOT_GPIO);
    return ESP_OK;
}

bool endstop_top_is_pressed(void)
{
    return !gpio_get_level(ENDSTOP_TOP_GPIO);   /* active-low */
}

bool endstop_bot_is_pressed(void)
{
    return !gpio_get_level(ENDSTOP_BOT_GPIO);   /* active-low */
}

bool endstop_is_pressed(void)
{
    return endstop_top_is_pressed();            /* backward-compatible alias */
}

esp_err_t endstop_register_callback(endstop_callback_t cb)
{
    endstop_top_cb = cb;
    return ESP_OK;
}

esp_err_t endstop_bot_register_callback(endstop_callback_t cb)
{
    endstop_bot_cb = cb;
    return ESP_OK;
}
