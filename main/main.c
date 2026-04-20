// main.c
#include "main.h"
#include "ui.h"
#include "bsp_angle.h"
#include "bsp_rtc.h"
#include "bsp_sdcard.h"
#include "bsp_audio.h"
#include "test_manager.h"
#include "audio_manager.h"
#include "data_logger.h"
#include "esp_lvgl_port.h"

/* LDO channel handles */
static esp_ldo_channel_handle_t ldo3 = NULL;
static esp_ldo_channel_handle_t ldo4 = NULL;

/**
 * @brief Initialization failure handler
 */
static void init_fail_handler(const char *module_name, esp_err_t err) {
    MAIN_ERROR("[%s] init failed: %s (continuing boot)", module_name, esp_err_to_name(err));
}

/**
 * @brief System initialization (LDO + LCD + backlight + peripherals)
 */
static void system_init(void) {
    esp_err_t err = ESP_OK;

    /* 1. Initialize LDO (required for screen power) */
    esp_ldo_channel_config_t ldo3_cof = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    err = esp_ldo_acquire_channel(&ldo3_cof, &ldo3);
    if (err != ESP_OK) init_fail_handler("ldo3", err);

    esp_ldo_channel_config_t ldo4_cof = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    err = esp_ldo_acquire_channel(&ldo4_cof, &ldo4);
    if (err != ESP_OK) init_fail_handler("ldo4", err);
    MAIN_INFO("LDO3 and LDO4 init success");

    /* 2. Initialize I2C (shared bus for touch, AS5600, DS3231) */
    MAIN_INFO("Initializing I2C...");
    err = i2c_init();
    if (err != ESP_OK) init_fail_handler("I2C", err);
    MAIN_INFO("I2C init success");

    /* 3. Initialize touch panel */
    MAIN_INFO("Initializing touch panel...");
    err = touch_init();
    if (err != ESP_OK) init_fail_handler("Touch", err);
    MAIN_INFO("Touch panel init success");

    /* 4. Initialize LCD hardware and LVGL */
    err = display_init();
    if (err != ESP_OK) init_fail_handler("LCD", err);
    MAIN_INFO("LCD init success");

    /* 5. Turn on LCD backlight */
    err = set_lcd_blight(100);
    if (err != ESP_OK) init_fail_handler("LCD Backlight", err);
    MAIN_INFO("LCD backlight opened (brightness: 100)");

    /* 6. Initialize NVS (for test_manager config storage) */
    MAIN_INFO("Initializing NVS...");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) init_fail_handler("NVS", err);
    MAIN_INFO("NVS init success");

    /* 7. Initialize SD card */
    MAIN_INFO("Initializing SD card...");
    err = sdcard_init();
    if (err != ESP_OK) {
        MAIN_ERROR("SD card init failed: %s (non-fatal)", esp_err_to_name(err));
    } else {
        MAIN_INFO("SD card mounted successfully");
    }

    /* 8. Initialize audio amplifier control + I2S */
    MAIN_INFO("Initializing audio...");
    err = audio_ctrl_init();
    if (err != ESP_OK) {
        MAIN_ERROR("Audio ctrl init failed: %s (non-fatal)", esp_err_to_name(err));
    }
    err = audio_init();
    if (err != ESP_OK) {
        MAIN_ERROR("Audio I2S init failed: %s (non-fatal)", esp_err_to_name(err));
    } else {
        MAIN_INFO("Audio init success");
    }

    /* 9. Initialize AS5600 angle sensor */
    MAIN_INFO("Initializing angle sensor...");
    err = angle_init();
    if (err != ESP_OK) {
        MAIN_ERROR("AS5600 init failed: %s (non-fatal)", esp_err_to_name(err));
    } else {
        MAIN_INFO("AS5600 init success");
    }

    /* 10. Initialize DS3231 RTC */
    MAIN_INFO("Initializing RTC...");
    err = rtc_init();
    if (err != ESP_OK) {
        MAIN_ERROR("DS3231 init failed: %s (non-fatal)", esp_err_to_name(err));
    } else {
        MAIN_INFO("DS3231 RTC init success");
    }

    /* 11. Initialize relay and endstop GPIOs */
    MAIN_INFO("Initializing relays and endstop...");
    err = relay_init();
    if (err != ESP_OK) init_fail_handler("Relay", err);
    err = endstop_init();
    if (err != ESP_OK) init_fail_handler("Endstop", err);
    MAIN_INFO("Relay and endstop init success");
}

/**
 * @brief State change callback — update UI from test manager
 */
static void on_test_state_change(test_state_t old_state, test_state_t new_state)
{
    (void)old_state;

    if (!lvgl_port_lock(100)) return;

    /* Update test active screen button states */
    ui_test_active_set_state((int)new_state);

    /* On complete, update results on test active screen */
    if (new_state == TEST_STATE_COMPLETE) {
        const test_result_t *result = test_manager_get_result();
        if (result) {
            char angle_buf[16], energy_buf[16];
            snprintf(angle_buf, sizeof(angle_buf), "%.1f°", result->final_angle_deg);
            snprintf(energy_buf, sizeof(energy_buf), "%.2f J", result->energy_joules);
            lv_label_set_text(ui_test_result_angle_label, angle_buf);
            lv_label_set_text(ui_test_result_energy_label, energy_buf);

            /* Also update dashboard last result */
            lv_label_set_text(ui_dash_last_angle_label, angle_buf);
            lv_label_set_text(ui_dash_last_energy_label, energy_buf);
            lv_label_set_text(ui_dash_last_specimen_label, result->specimen.specimen_id);
            lv_label_set_text(ui_dash_last_material_label, result->specimen.material);
            lv_label_set_text(ui_dash_last_operator_label, result->specimen.operator_name);
            lv_label_set_text(ui_dash_last_timestamp_label, result->timestamp);
        }
    }

    lvgl_port_unlock();
}

/**
 * @brief UI update task — reads sensors, updates status bar + live angle
 */
static void ui_update_task(void *pvParameters)
{
    (void)pvParameters;
    char time_buf[16], date_buf[16];
    rtc_datetime_t dt;

    while (1) {
        /* Read RTC time (outside lock — no LVGL dependency) */
        if (rtc_get_datetime(&dt) == ESP_OK) {
            rtc_format_time(&dt, time_buf, sizeof(time_buf));
            rtc_format_date(&dt, date_buf, sizeof(date_buf));
        } else {
            snprintf(time_buf, sizeof(time_buf), "--:--:--");
            snprintf(date_buf, sizeof(date_buf), "-- --- ----");
        }

        /* Determine state color (outside lock — no LVGL dependency) */
        test_state_t state = test_manager_get_state();
        lv_color_t dot_color = UI_COLOR_SUCCESS;
        if (state >= TEST_STATE_ARMING && state <= TEST_STATE_MEASURING) {
            dot_color = UI_COLOR_WARNING;
        } else if (state == TEST_STATE_ERROR) {
            dot_color = UI_COLOR_DANGER;
        }

        /* Read live angle (outside lock) */
        float angle = test_manager_get_live_angle();
        char angle_buf[16];
        snprintf(angle_buf, sizeof(angle_buf), "%.1f°", angle);
        int arc_val = (int)angle;
        if (arc_val < 0) arc_val = 0;
        if (arc_val > 360) arc_val = 360;

        /* Lock LVGL mutex for all UI updates */
        if (lvgl_port_lock(100)) {
            /* Update status bar on active screen */
            ui_update_status_bar(time_buf, date_buf,
                                 test_manager_state_name(state),
                                 dot_color, sdcard_is_mounted());

            /* Dashboard arc */
            if (ui_dash_arc) {
                lv_arc_set_value(ui_dash_arc, arc_val);
                lv_label_set_text(ui_dash_angle_label, angle_buf);
            }

            /* Test active arc (when on that screen) */
            if (ui_test_arc) {
                lv_arc_set_value(ui_test_arc, arc_val);
                lv_label_set_text(ui_test_angle_label, angle_buf);
            }

            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(33));  /* ~30 fps UI update */
    }
}

void app_main(void)
{
    MAIN_INFO("Starting Digital Charpy Impact Testing Machine...");

    /* System initialization (LDO, LCD, touch, I2C, peripherals) */
    system_init();
    MAIN_INFO("System initialized successfully");

    /* Initialize application modules */
    test_manager_init(NULL);
    MAIN_INFO("Test manager initialized");

    audio_manager_init();
    MAIN_INFO("Audio manager initialized");

    data_logger_init();
    data_logger_load_history();
    MAIN_INFO("Data logger initialized");

    /* Register state change callback */
    test_manager_register_state_cb(on_test_state_change);

    /* Register endstop callback with test manager */
    endstop_register_callback(test_manager_endstop_triggered);

    /* Initialize UI */
    ui_init();
    MAIN_INFO("UI initialized successfully");

    /* Create UI update task */
    xTaskCreate(ui_update_task, "ui_update", 4096, NULL, 3, NULL);
    MAIN_INFO("Application running");
}
