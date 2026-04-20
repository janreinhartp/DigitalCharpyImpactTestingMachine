#include "ui.h"
#include "test_manager.h"
#include "bsp_angle.h"
#include "bsp_rtc.h"
#include "bsp_sdcard.h"
#include "audio_manager.h"
#include <string.h>
#include <stdlib.h>

/* Settings widgets */
lv_obj_t *ui_set_mass_ta       = NULL;
lv_obj_t *ui_set_length_ta     = NULL;
lv_obj_t *ui_set_release_ta    = NULL;
lv_obj_t *ui_set_zero_label    = NULL;
lv_obj_t *ui_set_audio_sw      = NULL;
lv_obj_t *ui_set_sd_label      = NULL;
lv_obj_t *ui_set_rtc_label     = NULL;
lv_obj_t *ui_set_about_label   = NULL;

static lv_obj_t *content_panel = NULL;
static lv_obj_t *sidebar = NULL;
static lv_obj_t *settings_kb = NULL;

static int active_tab = 0;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

/* Forward declarations */
static void show_tab(int tab);

/* ——— Callbacks ——— */
static void btn_back_cb(lv_event_t *e) { (void)e; ui_show_dashboard(); }

static void btn_save_pendulum_cb(lv_event_t *e)
{
    (void)e;
    pendulum_config_t cfg;
    cfg.mass_kg        = (float)atof(lv_textarea_get_text(ui_set_mass_ta));
    cfg.arm_length_m   = (float)atof(lv_textarea_get_text(ui_set_length_ta));
    cfg.release_angle_deg = (float)atof(lv_textarea_get_text(ui_set_release_ta));
    test_manager_set_config(&cfg);
}

static void btn_calibrate_zero_cb(lv_event_t *e)
{
    (void)e;
    angle_calibrate_zero();
    float zero;
    angle_read_degrees(&zero);
    char buf[32];
    snprintf(buf, sizeof(buf), "Zero offset: %.1f\u00b0", zero);
    lv_label_set_text(ui_set_zero_label, buf);
}

static void audio_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    audio_manager_set_enabled(en);
}

static void set_ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (settings_kb) {
        lv_keyboard_set_textarea(settings_kb, ta);
        lv_obj_remove_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_kb_ready_cb(lv_event_t *e)
{
    (void)e;
    if (settings_kb) lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
}

/* ——— Sidebar tab buttons ——— */
static const char *tab_labels[] = {
    LV_SYMBOL_SETTINGS " Pendulum",
    LV_SYMBOL_REFRESH " Calibration",
    LV_SYMBOL_IMAGE " Date & Time",
    LV_SYMBOL_SD_CARD " SD Card",
    LV_SYMBOL_AUDIO " Audio",
    LV_SYMBOL_DUMMY " About"
};
#define TAB_COUNT 6

static lv_obj_t *tab_btns[TAB_COUNT] = {NULL};

static void tab_btn_cb(lv_event_t *e)
{
    int tab = (int)(intptr_t)lv_event_get_user_data(e);
    show_tab(tab);
}

/* ——— Create per-tab content ——— */
static lv_obj_t *create_labeled_input(lv_obj_t *parent, const char *label, const char *value, int w)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, w, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont, 4, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_obj_set_size(ta, lv_pct(100), 44);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, value);
    lv_textarea_set_accepted_chars(ta, "0123456789.");
    lv_obj_add_style(ta, &style_input, 0);
    lv_obj_add_event_cb(ta, set_ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    return ta;
}

static void create_tab_pendulum(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Pendulum Configuration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    const pendulum_config_t *cfg_ptr = test_manager_get_config();
    pendulum_config_t cfg = *cfg_ptr;
    char buf[16];

    snprintf(buf, sizeof(buf), "%.3f", cfg.mass_kg);
    ui_set_mass_ta = create_labeled_input(parent, "Hammer Mass (kg)", buf, 300);

    snprintf(buf, sizeof(buf), "%.3f", cfg.arm_length_m);
    ui_set_length_ta = create_labeled_input(parent, "Arm Length (m)", buf, 300);

    snprintf(buf, sizeof(buf), "%.1f", cfg.release_angle_deg);
    ui_set_release_ta = create_labeled_input(parent, "Release Angle (\u00b0)", buf, 300);

    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_add_style(btn_save, &style_btn_primary, 0);
    lv_obj_set_size(btn_save, 200, 48);
    lv_obj_add_event_cb(btn_save, btn_save_pendulum_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn_save);
    lv_label_set_text(lbl, LV_SYMBOL_SAVE " Save Config");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);
}

static void create_tab_calibration(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Angle Sensor Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    lv_obj_t *desc = lv_label_create(parent);
    lv_label_set_text(desc, "Position the pendulum arm at rest (0\u00b0) and press Calibrate.\n"
                            "The current sensor reading will be set as zero reference.");
    lv_obj_set_style_text_color(desc, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_16, 0);
    lv_obj_set_width(desc, lv_pct(100));

    ui_set_zero_label = lv_label_create(parent);
    lv_label_set_text(ui_set_zero_label, "Zero offset: --");
    lv_obj_set_style_text_color(ui_set_zero_label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(ui_set_zero_label, &lv_font_montserrat_20, 0);

    lv_obj_t *btn_cal = lv_btn_create(parent);
    lv_obj_add_style(btn_cal, &style_btn_primary, 0);
    lv_obj_set_size(btn_cal, 260, 48);
    lv_obj_add_event_cb(btn_cal, btn_calibrate_zero_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn_cal);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH " Calibrate Zero");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);
}

static void create_tab_datetime(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Date & Time (DS3231 RTC)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    ui_set_rtc_label = lv_label_create(parent);
    lv_label_set_text(ui_set_rtc_label, "Loading...");
    lv_obj_set_style_text_color(ui_set_rtc_label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(ui_set_rtc_label, &lv_font_montserrat_20, 0);

    /* Update with current time */
    rtc_datetime_t dt;
    if (rtc_get_datetime(&dt) == ESP_OK) {
        char tbuf[64];
        snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d  %02d:%02d:%02d",
                 dt.year, dt.month, dt.date, dt.hours, dt.minutes, dt.seconds);
        lv_label_set_text(ui_set_rtc_label, tbuf);
    } else {
        lv_label_set_text(ui_set_rtc_label, "RTC not connected");
    }

    lv_obj_t *note = lv_label_create(parent);
    lv_label_set_text(note, "Note: To set the time, power on the device with a\n"
                            "correctly set RTC module or use serial commands.");
    lv_obj_set_style_text_color(note, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
}

static void create_tab_sdcard(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "SD Card");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    ui_set_sd_label = lv_label_create(parent);
    lv_obj_set_style_text_color(ui_set_sd_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_set_sd_label, &lv_font_montserrat_20, 0);

    if (sdcard_is_mounted()) {
        uint64_t total_bytes, free_bytes;
        sdcard_get_space(&total_bytes, &free_bytes);
        uint32_t total_mb = (uint32_t)(total_bytes / (1024 * 1024));
        uint32_t free_mb = (uint32_t)(free_bytes / (1024 * 1024));
        char buf[128];
        snprintf(buf, sizeof(buf), "Status: Mounted\nTotal: %lu MB\nFree: %lu MB",
                 (unsigned long)total_mb, (unsigned long)free_mb);
        lv_label_set_text(ui_set_sd_label, buf);
    } else {
        lv_label_set_text(ui_set_sd_label, "Status: Not mounted\nInsert SD card and restart.");
    }
}

static void create_tab_audio(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Audio Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 16, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Sound Notifications");
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);

    ui_set_audio_sw = lv_switch_create(row);
    lv_obj_set_size(ui_set_audio_sw, 60, 30);
    if (audio_manager_is_enabled()) {
        lv_obj_add_state(ui_set_audio_sw, LV_STATE_CHECKED);
    }
    lv_obj_set_style_bg_color(ui_set_audio_sw, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_bg_color(ui_set_audio_sw, UI_COLOR_SUCCESS, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(ui_set_audio_sw, audio_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *note = lv_label_create(parent);
    lv_label_set_text(note, "Place WAV files in /sdcard/sounds/:\n"
                            "  armed.wav, released.wav, complete.wav,\n"
                            "  abort.wav, error.wav");
    lv_obj_set_style_text_color(note, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
}

static void create_tab_about(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "About");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    ui_set_about_label = lv_label_create(parent);
    lv_label_set_text(ui_set_about_label,
        "Digital Charpy Impact Testing Machine\n"
        "Version 1.0.0\n\n"
        "Platform: CrowPanel ESP32-P4 (9\" IPS)\n"
        "Framework: ESP-IDF 5.4.3 + LVGL 9.2.2\n\n"
        "Sensors:\n"
        "  - AS5600 Magnetic Angle Encoder\n"
        "  - DS3231 RTC (Battery Backed)\n"
        "  - GT911 Capacitive Touch\n\n"
        "Peripherals:\n"
        "  - SD Card (FAT32 data logging)\n"
        "  - I2S Audio (WAV playback)\n"
        "  - Dual Relay Control (Motor + Actuator)");
    lv_obj_set_style_text_color(ui_set_about_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_set_about_label, &lv_font_montserrat_16, 0);
}

static void show_tab(int tab)
{
    active_tab = tab;

    /* Highlight sidebar button */
    for (int i = 0; i < TAB_COUNT; i++) {
        if (tab_btns[i]) {
            if (i == tab) {
                lv_obj_set_style_bg_color(tab_btns[i], UI_COLOR_SURFACE_EL, 0);
                lv_obj_set_style_bg_opa(tab_btns[i], LV_OPA_COVER, 0);
            } else {
                lv_obj_set_style_bg_opa(tab_btns[i], LV_OPA_TRANSP, 0);
            }
        }
    }

    /* Clear and recreate content */
    lv_obj_clean(content_panel);
    lv_obj_set_flex_flow(content_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content_panel, 16, 0);

    switch (tab) {
        case 0: create_tab_pendulum(content_panel); break;
        case 1: create_tab_calibration(content_panel); break;
        case 2: create_tab_datetime(content_panel); break;
        case 3: create_tab_sdcard(content_panel); break;
        case 4: create_tab_audio(content_panel); break;
        case 5: create_tab_about(content_panel); break;
        default: break;
    }
}

void ui_settings_create(void)
{
    scr_settings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_settings, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_settings, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_settings, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_settings);

    /* Content area */
    lv_obj_t *content = lv_obj_create(scr_settings);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ——— Left sidebar (200px) ——— */
    sidebar = lv_obj_create(content);
    lv_obj_set_size(sidebar, 200, 540);
    lv_obj_set_pos(sidebar, 0, 0);
    lv_obj_set_style_bg_color(sidebar, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_border_color(sidebar, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_side(sidebar, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_width(sidebar, 1, 0);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sidebar, 8, 0);
    lv_obj_set_style_pad_row(sidebar, 4, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    /* Back button at top of sidebar */
    lv_obj_t *btn_back = lv_btn_create(sidebar);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, lv_pct(100), 40);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_lbl);

    /* Spacer */
    lv_obj_t *spacer = lv_obj_create(sidebar);
    lv_obj_set_size(spacer, lv_pct(100), 8);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);

    /* Tab buttons */
    for (int i = 0; i < TAB_COUNT; i++) {
        tab_btns[i] = lv_btn_create(sidebar);
        lv_obj_set_size(tab_btns[i], lv_pct(100), 44);
        lv_obj_set_style_bg_opa(tab_btns[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(tab_btns[i], 8, 0);
        lv_obj_set_style_shadow_width(tab_btns[i], 0, 0);
        lv_obj_set_style_border_width(tab_btns[i], 0, 0);
        lv_obj_add_event_cb(tab_btns[i], tab_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(tab_btns[i]);
        lv_label_set_text(lbl, tab_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
    }

    /* ——— Right content panel (824px) ——— */
    content_panel = lv_obj_create(content);
    lv_obj_set_size(content_panel, 804, 520);
    lv_obj_set_pos(content_panel, 210, 10);
    lv_obj_add_style(content_panel, &style_card, 0);
    lv_obj_set_style_pad_all(content_panel, 24, 0);

    /* On-screen keyboard (hidden) */
    settings_kb = lv_keyboard_create(scr_settings);
    lv_obj_set_size(settings_kb, 1024, 220);
    lv_obj_align(settings_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(settings_kb, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_color(settings_kb, UI_COLOR_SURFACE_EL, LV_PART_ITEMS);
    lv_obj_set_style_text_color(settings_kb, UI_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_add_event_cb(settings_kb, set_kb_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(settings_kb, set_kb_ready_cb, LV_EVENT_CANCEL, NULL);
    lv_keyboard_set_mode(settings_kb, LV_KEYBOARD_MODE_NUMBER);

    /* Show first tab */
    show_tab(0);
}
