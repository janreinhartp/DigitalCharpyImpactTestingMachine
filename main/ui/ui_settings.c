#include "ui.h"
#include "test_manager.h"
#include "bsp_angle.h"
#include "bsp_rtc.h"
#include "bsp_sdcard.h"
#include "bsp_servo.h"
#include "bsp_extra.h"
#include "audio_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

/* ══════════════════════════════════════════════════════
 * Angle calibration — 3-point widgets
 * ══════════════════════════════════════════════════════ */
static lv_obj_t *s_cal_step1_lbl = NULL;   /* step-1 status */
static lv_obj_t *s_cal_step2_lbl = NULL;   /* step-2 status */
static lv_obj_t *s_cal_step3_lbl = NULL;   /* step-3 status */
static lv_obj_t *s_cal_arm_ta    = NULL;   /* arm-pos angle input  */
static lv_obj_t *s_cal_max_ta    = NULL;   /* max-swing angle input */

static void cal_update_apply_btn(lv_obj_t *btn)
{
    /* Enable the Apply button only when all 3 points are captured */
    if (angle_cal_is_complete()) {
        lv_obj_remove_state(btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    }
}

static lv_obj_t *s_cal_apply_btn = NULL;   /* kept so callbacks can refresh it */

static void cal_capture_zero_cb(lv_event_t *e)
{
    (void)e;
    if (angle_cal_capture_zero() == ESP_OK) {
        uint16_t raw = angle_cal_get_raw_zero();  /* value just captured from sensor */
        char buf[32];
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " OK  (raw=%u)", raw);
        lv_label_set_text(s_cal_step1_lbl, buf);
        lv_obj_set_style_text_color(s_cal_step1_lbl, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(s_cal_step1_lbl, LV_SYMBOL_CLOSE " Read error");
        lv_obj_set_style_text_color(s_cal_step1_lbl, UI_COLOR_DANGER, 0);
    }
    if (s_cal_apply_btn) cal_update_apply_btn(s_cal_apply_btn);
}

static void cal_capture_arm_cb(lv_event_t *e)
{
    (void)e;
    if (!s_cal_arm_ta) return;
    float arm_deg = (float)atof(lv_textarea_get_text(s_cal_arm_ta));
    if (angle_cal_capture_arm(arm_deg) == ESP_OK) {
        char buf[40];
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " OK  (%.1f\u00b0 entered)", arm_deg);
        lv_label_set_text(s_cal_step2_lbl, buf);
        lv_obj_set_style_text_color(s_cal_step2_lbl, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(s_cal_step2_lbl, LV_SYMBOL_CLOSE " Read error");
        lv_obj_set_style_text_color(s_cal_step2_lbl, UI_COLOR_DANGER, 0);
    }
    if (s_cal_apply_btn) cal_update_apply_btn(s_cal_apply_btn);
}

static void cal_capture_max_cb(lv_event_t *e)
{
    (void)e;
    if (!s_cal_max_ta) return;
    float max_deg = (float)atof(lv_textarea_get_text(s_cal_max_ta));
    if (angle_cal_capture_max(max_deg) == ESP_OK) {
        char buf[40];
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " OK  (%.1f\u00b0 entered)", max_deg);
        lv_label_set_text(s_cal_step3_lbl, buf);
        lv_obj_set_style_text_color(s_cal_step3_lbl, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(s_cal_step3_lbl, LV_SYMBOL_CLOSE " Read error");
        lv_obj_set_style_text_color(s_cal_step3_lbl, UI_COLOR_DANGER, 0);
    }
    if (s_cal_apply_btn) cal_update_apply_btn(s_cal_apply_btn);
}

static void cal_apply_save_cb(lv_event_t *e)
{
    (void)e;
    float sf = 1.0f;
    esp_err_t err = angle_cal_apply(&sf);
    if (err == ESP_OK) {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 LV_SYMBOL_OK " Scale: %.4f | Zero: %u raw",
                 sf, angle_get_zero_offset());
        lv_label_set_text(ui_set_zero_label, buf);
        lv_obj_set_style_text_color(ui_set_zero_label, UI_COLOR_SUCCESS, 0);
        test_manager_save_config();   /* persist to NVS */
    } else {
        lv_label_set_text(ui_set_zero_label,
                          LV_SYMBOL_CLOSE " Apply failed: capture all 3 points first");
        lv_obj_set_style_text_color(ui_set_zero_label, UI_COLOR_DANGER, 0);
    }
}

/* ——— Brake servo calibration ——— */
static lv_obj_t *s_brake_angle_label  = NULL;
static lv_obj_t *s_brake_test_status  = NULL;
static lv_obj_t *s_brake_hold_ta      = NULL;
static float     s_brake_preview     = 90.0f;

static void update_brake_label(void)
{
    if (!s_brake_angle_label) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "Target angle: %.1f\u00b0", s_brake_preview);
    lv_label_set_text(s_brake_angle_label, buf);
}

static void btn_servo_zero_cb(lv_event_t *e)
{
    (void)e;
    /* Move servo to home physically — do NOT change the saved target angle */
    servo_set_angle(90.0f);
}

static void btn_servo_minus_cb(lv_event_t *e)
{
    (void)e;
    s_brake_preview -= 1.0f;
    if (s_brake_preview < 0.0f) s_brake_preview = 0.0f;
    servo_set_angle(s_brake_preview);
    update_brake_label();
}

static void btn_servo_plus_cb(lv_event_t *e)
{
    (void)e;
    s_brake_preview += 1.0f;
    if (s_brake_preview > 180.0f) s_brake_preview = 180.0f;
    servo_set_angle(s_brake_preview);
    update_brake_label();
}

static void btn_servo_save_cb(lv_event_t *e)
{
    (void)e;
    test_manager_set_brake_angle(s_brake_preview);
}

static void btn_brake_hold_save_cb(lv_event_t *e)
{
    (void)e;
    if (!s_brake_hold_ta) return;
    uint32_t ms = (uint32_t)atoi(lv_textarea_get_text(s_brake_hold_ta));
    if (test_manager_set_brake_hold_ms(ms) == ESP_OK) {
        if (s_brake_test_status) {
            char buf[48];
            snprintf(buf, sizeof(buf), LV_SYMBOL_OK " Hold time saved: %lu ms", (unsigned long)ms);
            lv_label_set_text(s_brake_test_status, buf);
        }
    } else {
        if (s_brake_test_status)
            lv_label_set_text(s_brake_test_status, LV_SYMBOL_WARNING " Invalid value (100–30000 ms)");
    }
}

static void ui_brake_retract_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(test_manager_get_brake_hold_ms()));
    servo_jog_to_angle(90.0f, 2000);
    vTaskDelete(NULL);
}

static void btn_servo_test_cb(lv_event_t *e)
{
    (void)e;
    /* Fast move to brake angle */
    servo_set_angle(s_brake_preview);

    /* Background task: wait hold time, then jog back home */
    xTaskCreate(ui_brake_retract_task, "ui_brk_ret", 4096, NULL, 3, NULL);

    if (s_brake_test_status) {
        char buf[64];
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " Moved to %.0f\u00b0 \u2014 retracting in %lu ms",
                 s_brake_preview, (unsigned long)test_manager_get_brake_hold_ms());
        lv_label_set_text(s_brake_test_status, buf);
    }
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
    LV_SYMBOL_WARNING " Machine Test",
    LV_SYMBOL_DUMMY " About"
};
#define TAB_COUNT 7

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
    /* Reset widget handles */
    s_cal_step1_lbl = NULL;
    s_cal_step2_lbl = NULL;
    s_cal_step3_lbl = NULL;
    s_cal_arm_ta    = NULL;
    s_cal_max_ta    = NULL;
    s_cal_apply_btn = NULL;

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Angle Sensor Calibration (3-Point)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    lv_obj_t *desc = lv_label_create(parent);
    lv_label_set_text(desc,
        "A conversion pulley changes the sensor-to-arm ratio.\n"
        "Capture 3 positions to compute the scale factor automatically.");
    lv_obj_set_style_text_color(desc, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_16, 0);
    lv_obj_set_width(desc, lv_pct(100));

    /* Helper: create a numbered step card */
#define MAKE_STEP_CARD(num, header_text) \
    lv_obj_t *card##num = lv_obj_create(parent); \
    lv_obj_set_size(card##num, lv_pct(100), LV_SIZE_CONTENT); \
    lv_obj_set_style_bg_color(card##num, UI_COLOR_SURFACE, 0); \
    lv_obj_set_style_bg_opa(card##num, LV_OPA_COVER, 0); \
    lv_obj_set_style_radius(card##num, 8, 0); \
    lv_obj_set_style_border_color(card##num, UI_COLOR_BORDER, 0); \
    lv_obj_set_style_border_width(card##num, 1, 0); \
    lv_obj_set_style_pad_all(card##num, 12, 0); \
    lv_obj_set_flex_flow(card##num, LV_FLEX_FLOW_COLUMN); \
    lv_obj_set_style_pad_row(card##num, 8, 0); \
    lv_obj_clear_flag(card##num, LV_OBJ_FLAG_SCROLLABLE); \
    lv_obj_t *hdr##num = lv_label_create(card##num); \
    lv_label_set_text(hdr##num, header_text); \
    lv_obj_set_style_text_font(hdr##num, &lv_font_montserrat_20, 0); \
    lv_obj_set_style_text_color(hdr##num, UI_COLOR_PRIMARY, 0)

    /* ── Step 1: Home / Zero ── */
    MAKE_STEP_CARD(1, LV_SYMBOL_HOME " Step 1 - Home (0 deg)");

    lv_obj_t *desc1 = lv_label_create(card1);
    lv_label_set_text(desc1, "Position the arm straight down at rest, then capture.");
    lv_obj_set_style_text_color(desc1, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(desc1, &lv_font_montserrat_16, 0);

    lv_obj_t *btn1 = lv_btn_create(card1);
    lv_obj_add_style(btn1, &style_btn_primary, 0);
    lv_obj_set_size(btn1, 200, 44);
    lv_obj_add_event_cb(btn1, cal_capture_zero_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl1 = lv_label_create(btn1);
    lv_label_set_text(lbl1, LV_SYMBOL_DOWNLOAD " Capture Zero");
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl1);

    s_cal_step1_lbl = lv_label_create(card1);
    lv_label_set_text(s_cal_step1_lbl, "Pending...");
    lv_obj_set_style_text_color(s_cal_step1_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_cal_step1_lbl, &lv_font_montserrat_16, 0);

    /* ── Step 2: Arm position ── */
    MAKE_STEP_CARD(2, LV_SYMBOL_UP " Step 2 - Arm Position");

    lv_obj_t *desc2 = lv_label_create(card2);
    lv_label_set_text(desc2,
        "Raise the arm to the known release position.\n"
        "Enter the exact arm angle below, then capture.");
    lv_obj_set_style_text_color(desc2, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(desc2, &lv_font_montserrat_16, 0);

    /* Angle input row */
    lv_obj_t *row2 = lv_obj_create(card2);
    lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row2, 8, 0);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *inp_lbl2 = lv_label_create(row2);
    lv_label_set_text(inp_lbl2, "Angle (deg):");
    lv_obj_set_style_text_font(inp_lbl2, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(inp_lbl2, UI_COLOR_TEXT, 0);

    s_cal_arm_ta = lv_textarea_create(row2);
    lv_obj_set_size(s_cal_arm_ta, 120, 44);
    lv_textarea_set_one_line(s_cal_arm_ta, true);
    lv_textarea_set_text(s_cal_arm_ta, "150.0");
    lv_textarea_set_accepted_chars(s_cal_arm_ta, "0123456789.-");
    lv_obj_add_style(s_cal_arm_ta, &style_input, 0);
    lv_obj_add_event_cb(s_cal_arm_ta, set_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btn2 = lv_btn_create(card2);
    lv_obj_add_style(btn2, &style_btn_primary, 0);
    lv_obj_set_size(btn2, 200, 44);
    lv_obj_add_event_cb(btn2, cal_capture_arm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl2 = lv_label_create(btn2);
    lv_label_set_text(lbl2, LV_SYMBOL_DOWNLOAD " Capture Arm");
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl2);

    s_cal_step2_lbl = lv_label_create(card2);
    lv_label_set_text(s_cal_step2_lbl, "Pending...");
    lv_obj_set_style_text_color(s_cal_step2_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_cal_step2_lbl, &lv_font_montserrat_16, 0);

    /* ── Step 3: Max swing ── */
    MAKE_STEP_CARD(3, LV_SYMBOL_DOWN " Step 3 - Max Swing");

    lv_obj_t *desc3 = lv_label_create(card3);
    lv_label_set_text(desc3,
        "Move the arm to the maximum swing position (other side).\n"
        "Enter the arm angle (negative for opposite side), then capture.");
    lv_obj_set_style_text_color(desc3, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(desc3, &lv_font_montserrat_16, 0);

    /* Angle input row */
    lv_obj_t *row3 = lv_obj_create(card3);
    lv_obj_set_size(row3, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row3, 8, 0);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row3, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *inp_lbl3 = lv_label_create(row3);
    lv_label_set_text(inp_lbl3, "Angle (deg):");
    lv_obj_set_style_text_font(inp_lbl3, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(inp_lbl3, UI_COLOR_TEXT, 0);

    s_cal_max_ta = lv_textarea_create(row3);
    lv_obj_set_size(s_cal_max_ta, 120, 44);
    lv_textarea_set_one_line(s_cal_max_ta, true);
    lv_textarea_set_text(s_cal_max_ta, "-150.0");
    lv_textarea_set_accepted_chars(s_cal_max_ta, "0123456789.-");
    lv_obj_add_style(s_cal_max_ta, &style_input, 0);
    lv_obj_add_event_cb(s_cal_max_ta, set_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btn3 = lv_btn_create(card3);
    lv_obj_add_style(btn3, &style_btn_primary, 0);
    lv_obj_set_size(btn3, 200, 44);
    lv_obj_add_event_cb(btn3, cal_capture_max_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl3 = lv_label_create(btn3);
    lv_label_set_text(lbl3, LV_SYMBOL_DOWNLOAD " Capture Max Swing");
    lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl3);

    s_cal_step3_lbl = lv_label_create(card3);
    lv_label_set_text(s_cal_step3_lbl, "Pending...");
    lv_obj_set_style_text_color(s_cal_step3_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_cal_step3_lbl, &lv_font_montserrat_16, 0);

    /* ── Apply & Save ── */
    s_cal_apply_btn = lv_btn_create(parent);
    lv_obj_add_style(s_cal_apply_btn, &style_btn_danger, 0);
    lv_obj_set_size(s_cal_apply_btn, 280, 52);
    lv_obj_add_state(s_cal_apply_btn, LV_STATE_DISABLED);   /* enabled after all 3 captured */
    lv_obj_add_event_cb(s_cal_apply_btn, cal_apply_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *apply_lbl = lv_label_create(s_cal_apply_btn);
    lv_label_set_text(apply_lbl, LV_SYMBOL_SAVE " Apply & Save Calibration");
    lv_obj_set_style_text_font(apply_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(apply_lbl);

    /* Result label (reuses ui_set_zero_label so it's accessible globally) */
    ui_set_zero_label = lv_label_create(parent);
    char init_buf[48];
    snprintf(init_buf, sizeof(init_buf),
             "Current scale: %.4f  |  Zero: %u raw",
             angle_get_scale(), angle_get_zero_offset());
    lv_label_set_text(ui_set_zero_label, init_buf);
    lv_obj_set_style_text_color(ui_set_zero_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_set_zero_label, &lv_font_montserrat_16, 0);

#undef MAKE_STEP_CARD

    /* ——— Divider ——— */
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_set_size(div, lv_pct(100), 1);
    lv_obj_set_style_bg_color(div, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);

    /* ——— Brake Servo Calibration ——— */
    lv_obj_t *srv_title = lv_label_create(parent);
    lv_label_set_text(srv_title, "Brake Servo Calibration");
    lv_obj_set_style_text_font(srv_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(srv_title, UI_COLOR_TEXT, 0);

    lv_obj_t *srv_desc = lv_label_create(parent);
    lv_label_set_text(srv_desc, "Use + / \xe2\x88\x92 to adjust the brake position, then press Save.");
    lv_obj_set_style_text_color(srv_desc, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(srv_desc, &lv_font_montserrat_16, 0);
    lv_obj_set_width(srv_desc, lv_pct(100));

    /* Initialise preview from saved value and move servo there */
    s_brake_preview = test_manager_get_brake_angle();
    servo_set_angle(s_brake_preview);

    s_brake_angle_label = lv_label_create(parent);
    lv_obj_set_style_text_color(s_brake_angle_label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_brake_angle_label, &lv_font_montserrat_20, 0);
    update_brake_label();

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(parent);
    lv_obj_set_size(btn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Return to Zero */
    lv_obj_t *btn_zero = lv_btn_create(btn_row);
    lv_obj_add_style(btn_zero, &style_btn_secondary, 0);
    lv_obj_set_size(btn_zero, 180, 48);
    lv_obj_add_event_cb(btn_zero, btn_servo_zero_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_zero = lv_label_create(btn_zero);
    lv_label_set_text(lbl_zero, LV_SYMBOL_HOME " Zero");
    lv_obj_set_style_text_font(lbl_zero, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_zero);

    /* Minus */
    lv_obj_t *btn_minus = lv_btn_create(btn_row);
    lv_obj_add_style(btn_minus, &style_btn_secondary, 0);
    lv_obj_set_size(btn_minus, 100, 48);
    lv_obj_add_event_cb(btn_minus, btn_servo_minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, LV_SYMBOL_MINUS " 1°");
    lv_obj_set_style_text_font(lbl_minus, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_minus);

    /* Plus */
    lv_obj_t *btn_plus = lv_btn_create(btn_row);
    lv_obj_add_style(btn_plus, &style_btn_secondary, 0);
    lv_obj_set_size(btn_plus, 100, 48);
    lv_obj_add_event_cb(btn_plus, btn_servo_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, LV_SYMBOL_PLUS " 1°");
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_plus);

    /* Save */
    lv_obj_t *btn_save = lv_btn_create(btn_row);
    lv_obj_add_style(btn_save, &style_btn_primary, 0);
    lv_obj_set_size(btn_save, 140, 48);
    lv_obj_add_event_cb(btn_save, btn_servo_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_SAVE " Save");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_save);

    /* Test Brake */
    lv_obj_t *btn_test = lv_btn_create(btn_row);
    lv_obj_add_style(btn_test, &style_btn_warning, 0);
    lv_obj_set_size(btn_test, 180, 48);
    lv_obj_add_event_cb(btn_test, btn_servo_test_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_test = lv_label_create(btn_test);
    lv_label_set_text(lbl_test, LV_SYMBOL_RIGHT " Move to Angle");
    lv_obj_set_style_text_font(lbl_test, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_test);

    /* Test status feedback label */
    s_brake_test_status = lv_label_create(parent);
    lv_label_set_text(s_brake_test_status, "Press \"Test Brake\" to verify repeatability.");
    lv_obj_set_style_text_color(s_brake_test_status, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_brake_test_status, &lv_font_montserrat_16, 0);
    lv_obj_set_width(s_brake_test_status, lv_pct(100));

    /* ——— Brake Hold Time ——— */
    lv_obj_t *hold_row = lv_obj_create(parent);
    lv_obj_set_size(hold_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hold_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hold_row, 0, 0);
    lv_obj_set_style_pad_all(hold_row, 0, 0);
    lv_obj_set_flex_flow(hold_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(hold_row, 12, 0);
    lv_obj_set_flex_align(hold_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hold_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hold_lbl = lv_label_create(hold_row);
    lv_label_set_text(hold_lbl, "Brake Hold Time (ms):");
    lv_obj_set_style_text_font(hold_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hold_lbl, UI_COLOR_TEXT, 0);

    char hold_buf[16];
    snprintf(hold_buf, sizeof(hold_buf), "%lu", (unsigned long)test_manager_get_brake_hold_ms());
    s_brake_hold_ta = lv_textarea_create(hold_row);
    lv_obj_set_size(s_brake_hold_ta, 140, 44);
    lv_textarea_set_one_line(s_brake_hold_ta, true);
    lv_textarea_set_text(s_brake_hold_ta, hold_buf);
    lv_textarea_set_accepted_chars(s_brake_hold_ta, "0123456789");
    lv_obj_add_style(s_brake_hold_ta, &style_input, 0);
    lv_obj_add_event_cb(s_brake_hold_ta, set_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btn_hold_save = lv_btn_create(hold_row);
    lv_obj_add_style(btn_hold_save, &style_btn_primary, 0);
    lv_obj_set_size(btn_hold_save, 140, 48);
    lv_obj_add_event_cb(btn_hold_save, btn_brake_hold_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_hold_save = lv_label_create(btn_hold_save);
    lv_label_set_text(lbl_hold_save, LV_SYMBOL_SAVE " Save");
    lv_obj_set_style_text_font(lbl_hold_save, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_hold_save);
}

/* ——— Date/time input text-area handles (file-scope so callbacks reach them) ——— */
static lv_obj_t *s_ta_year  = NULL;
static lv_obj_t *s_ta_month = NULL;
static lv_obj_t *s_ta_day   = NULL;
static lv_obj_t *s_ta_hour  = NULL;
static lv_obj_t *s_ta_min   = NULL;
static lv_obj_t *s_ta_sec   = NULL;
static lv_obj_t *s_dt_status = NULL;  /* feedback label */

static void dt_ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (settings_kb) {
        lv_keyboard_set_textarea(settings_kb, ta);
        lv_obj_remove_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void btn_set_manual_cb(lv_event_t *e)
{
    (void)e;
    rtc_datetime_t dt = {0};
    dt.year        = (uint16_t)atoi(lv_textarea_get_text(s_ta_year));
    dt.month       = (uint8_t) atoi(lv_textarea_get_text(s_ta_month));
    dt.date        = (uint8_t) atoi(lv_textarea_get_text(s_ta_day));
    dt.hours       = (uint8_t) atoi(lv_textarea_get_text(s_ta_hour));
    dt.minutes     = (uint8_t) atoi(lv_textarea_get_text(s_ta_min));
    dt.seconds     = (uint8_t) atoi(lv_textarea_get_text(s_ta_sec));
    dt.day_of_week = 1;

    /* Basic range validation */
    if (dt.year < 2020 || dt.year > 2099 ||
        dt.month < 1   || dt.month > 12  ||
        dt.date < 1    || dt.date > 31   ||
        dt.hours > 23  || dt.minutes > 59 || dt.seconds > 59) {
        lv_label_set_text(s_dt_status, "Invalid values — check ranges");
        lv_obj_set_style_text_color(s_dt_status, UI_COLOR_DANGER, 0);
        return;
    }

    if (rtc_set_datetime(&dt) == ESP_OK) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d  \xe2\x9c\x93 Set OK",
                 dt.year, dt.month, dt.date, dt.hours, dt.minutes, dt.seconds);
        lv_label_set_text(s_dt_status, buf);
        lv_obj_set_style_text_color(s_dt_status, UI_COLOR_SUCCESS, 0);
        if (ui_set_rtc_label) lv_label_set_text(ui_set_rtc_label, buf);
    } else {
        lv_label_set_text(s_dt_status, "RTC write failed - check wiring");
        lv_obj_set_style_text_color(s_dt_status, UI_COLOR_DANGER, 0);
    }

    if (settings_kb) lv_obj_add_flag(settings_kb, LV_OBJ_FLAG_HIDDEN);
}

static void btn_set_buildtime_cb(lv_event_t *e)
{
    (void)e;

    /* Parse __DATE__ / __TIME__ (same logic as main.c) */
    static const char *months[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    const char *d = __DATE__;
    const char *t = __TIME__;
    rtc_datetime_t dt = {0};

    char mon[4] = {d[0], d[1], d[2], '\0'};
    dt.month = 1;
    for (int i = 0; i < 12; i++) {
        if (strncmp(mon, months[i], 3) == 0) { dt.month = (uint8_t)(i + 1); break; }
    }
    dt.date        = (uint8_t)atoi(d + 4);
    dt.year        = (uint16_t)atoi(d + 7);
    dt.hours       = (uint8_t)atoi(t);
    dt.minutes     = (uint8_t)atoi(t + 3);
    dt.seconds     = (uint8_t)atoi(t + 6);
    dt.day_of_week = 1;

    if (rtc_set_datetime(&dt) == ESP_OK) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d  \xe2\x9c\x93 Set OK",
                 dt.year, dt.month, dt.date, dt.hours, dt.minutes, dt.seconds);
        lv_label_set_text(s_dt_status, buf);
        lv_obj_set_style_text_color(s_dt_status, UI_COLOR_SUCCESS, 0);
        if (ui_set_rtc_label) lv_label_set_text(ui_set_rtc_label, buf);
        /* Mirror into the manual inputs */
        if (s_ta_year) { char b[8]; snprintf(b,sizeof(b),"%04d",dt.year);    lv_textarea_set_text(s_ta_year,  b); }
        if (s_ta_month){ char b[4]; snprintf(b,sizeof(b),"%02d",dt.month);   lv_textarea_set_text(s_ta_month, b); }
        if (s_ta_day)  { char b[4]; snprintf(b,sizeof(b),"%02d",dt.date);    lv_textarea_set_text(s_ta_day,   b); }
        if (s_ta_hour) { char b[4]; snprintf(b,sizeof(b),"%02d",dt.hours);   lv_textarea_set_text(s_ta_hour,  b); }
        if (s_ta_min)  { char b[4]; snprintf(b,sizeof(b),"%02d",dt.minutes); lv_textarea_set_text(s_ta_min,   b); }
        if (s_ta_sec)  { char b[4]; snprintf(b,sizeof(b),"%02d",dt.seconds); lv_textarea_set_text(s_ta_sec,   b); }
    } else {
        lv_label_set_text(s_dt_status, "RTC write failed - check wiring");
        lv_obj_set_style_text_color(s_dt_status, UI_COLOR_DANGER, 0);
    }
}

/* Helper: numeric text-area with a label above it */
static lv_obj_t *make_dt_field(lv_obj_t *parent, const char *label_text,
                                int w, const char *default_val)
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
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SEC, 0);

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_obj_set_size(ta, lv_pct(100), 44);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_accepted_chars(ta, "0123456789");
    lv_textarea_set_max_length(ta, 4);
    if (default_val) lv_textarea_set_text(ta, default_val);
    lv_obj_add_style(ta, &style_input, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta, dt_ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    return ta;
}

static void create_tab_datetime(lv_obj_t *parent)
{
    /* Reset file-scope handles */
    s_ta_year = s_ta_month = s_ta_day = NULL;
    s_ta_hour = s_ta_min   = s_ta_sec = NULL;
    s_dt_status = NULL;

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Date & Time (DS1307 RTC)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    /* ── Current RTC time ── */
    ui_set_rtc_label = lv_label_create(parent);
    lv_obj_set_style_text_color(ui_set_rtc_label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(ui_set_rtc_label, &lv_font_montserrat_20, 0);

    rtc_datetime_t dt;
    char cur_time_buf[32] = "RTC not connected";
    if (rtc_get_datetime(&dt) == ESP_OK) {
        snprintf(cur_time_buf, sizeof(cur_time_buf), "%04d-%02d-%02d  %02d:%02d:%02d",
                 dt.year, dt.month, dt.date, dt.hours, dt.minutes, dt.seconds);
    }
    lv_label_set_text(ui_set_rtc_label, cur_time_buf);

    /* ── Manual entry section label ── */
    lv_obj_t *sec_lbl = lv_label_create(parent);
    lv_label_set_text(sec_lbl, "Set Date & Time manually:");
    lv_obj_set_style_text_color(sec_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(sec_lbl, &lv_font_montserrat_16, 0);

    /* ── Date row: YYYY  MM  DD ── */
    lv_obj_t *date_row = lv_obj_create(parent);
    lv_obj_set_size(date_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_row, 0, 0);
    lv_obj_set_style_pad_all(date_row, 0, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(date_row, 12, 0);
    lv_obj_clear_flag(date_row, LV_OBJ_FLAG_SCROLLABLE);

    char yy[8]="", mm[4]="", dd[4]="", hh[4]="", mi[4]="", ss[4]="";
    if (rtc_get_datetime(&dt) == ESP_OK) {
        snprintf(yy, sizeof(yy), "%04d", dt.year);
        snprintf(mm, sizeof(mm), "%02d", dt.month);
        snprintf(dd, sizeof(dd), "%02d", dt.date);
        snprintf(hh, sizeof(hh), "%02d", dt.hours);
        snprintf(mi, sizeof(mi), "%02d", dt.minutes);
        snprintf(ss, sizeof(ss), "%02d", dt.seconds);
    }

    s_ta_year  = make_dt_field(date_row, "Year (YYYY)",  120, yy[0] ? yy : "2026");
    s_ta_month = make_dt_field(date_row, "Month (1-12)",  90, mm[0] ? mm : "01");
    s_ta_day   = make_dt_field(date_row, "Day (1-31)",    90, dd[0] ? dd : "01");

    /* ── Time row: HH  MM  SS ── */
    lv_obj_t *time_row = lv_obj_create(parent);
    lv_obj_set_size(time_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(time_row, 12, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);

    s_ta_hour = make_dt_field(time_row, "Hour (0-23)",   90, hh[0] ? hh : "00");
    s_ta_min  = make_dt_field(time_row, "Minute (0-59)", 90, mi[0] ? mi : "00");
    s_ta_sec  = make_dt_field(time_row, "Second (0-59)", 90, ss[0] ? ss : "00");

    /* ── Feedback / status label ── */
    s_dt_status = lv_label_create(parent);
    lv_label_set_text(s_dt_status, "");
    lv_obj_set_style_text_font(s_dt_status, &lv_font_montserrat_16, 0);

    /* ── Button row ── */
    lv_obj_t *btn_row = lv_obj_create(parent);
    lv_obj_set_size(btn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 16, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_manual = lv_btn_create(btn_row);
    lv_obj_add_style(btn_manual, &style_btn_primary, 0);
    lv_obj_set_size(btn_manual, 220, 48);
    lv_obj_add_event_cb(btn_manual, btn_set_manual_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ml = lv_label_create(btn_manual);
    lv_label_set_text(ml, LV_SYMBOL_OK "  Apply Time");
    lv_obj_set_style_text_font(ml, &lv_font_montserrat_16, 0);
    lv_obj_center(ml);

    lv_obj_t *btn_build = lv_btn_create(btn_row);
    lv_obj_add_style(btn_build, &style_btn_secondary, 0);
    lv_obj_set_size(btn_build, 260, 48);
    lv_obj_add_event_cb(btn_build, btn_set_buildtime_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn_build);
    lv_label_set_text(bl, LV_SYMBOL_REFRESH "  Use Build Time");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);
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
        "  - Motor Control (Forward / Reverse / Release / Safety Lock)");
    lv_obj_set_style_text_color(ui_set_about_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_set_about_label, &lv_font_montserrat_16, 0);
}

/* ═══════════════════════════════════════════════════════════
 * Machine Test / Jog tab
 * ═══════════════════════════════════════════════════════════ */

/* File-scope widget handles — reset on every tab open */
static lv_obj_t *s_jog_motor_lbl    = NULL;
static lv_obj_t *s_jog_act_lbl      = NULL;
static lv_obj_t *s_jog_lock_lbl     = NULL;
static lv_obj_t *s_jog_servo_lbl    = NULL;
static lv_obj_t *s_jog_endstop_lbl  = NULL;
static lv_obj_t *s_jog_endstop_bot_lbl = NULL;
static lv_timer_t *s_endstop_poll_timer = NULL;
static float     s_jog_servo_angle  = 90.0f;

static void jog_update_motor_label(void)
{
    if (!s_jog_motor_lbl) return;
    bool fwd = motor_forward_get();
    bool rev = motor_reverse_get();
    const char *state = fwd ? "FWD" : (rev ? "REV" : "OFF");
    lv_label_set_text(s_jog_motor_lbl, fwd ? "State: FWD" : (rev ? "State: REV" : "State: OFF"));
    lv_obj_set_style_text_color(s_jog_motor_lbl,
        (fwd || rev) ? UI_COLOR_DANGER : UI_COLOR_SUCCESS, 0);
    (void)state;
}

static void jog_update_actuator_label(void)
{
    if (!s_jog_act_lbl) return;
    bool on = release_get();
    lv_label_set_text(s_jog_act_lbl, on ? "State: ON" : "State: OFF");
    lv_obj_set_style_text_color(s_jog_act_lbl,
        on ? UI_COLOR_DANGER : UI_COLOR_SUCCESS, 0);
}

static void jog_update_lock_label(void)
{
    if (!s_jog_lock_lbl) return;
    bool engaged = safety_lock_get();
    lv_label_set_text(s_jog_lock_lbl, engaged ? "State: ENGAGED" : "State: RELEASED");
    lv_obj_set_style_text_color(s_jog_lock_lbl,
        engaged ? UI_COLOR_SUCCESS : UI_COLOR_WARNING, 0);
}

static void jog_update_servo_label(void)
{
    if (!s_jog_servo_lbl) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "Position: %.1f\u00b0", s_jog_servo_angle);
    lv_label_set_text(s_jog_servo_lbl, buf);
}

static void jog_motor_on_cb(lv_event_t *e)
{
    (void)e;
    /* Safety lock stays ENGAGED during forward — it is a one-way hold.
     * Only reverse releases it. */
    motor_forward_set(true);
    jog_update_motor_label();
    jog_update_lock_label();
}

static void jog_motor_off_cb(lv_event_t *e)
{
    (void)e;
    motor_forward_set(false);
    motor_reverse_set(false);
    safety_lock_set(true);
    jog_update_motor_label();
    jog_update_lock_label();
}

static void jog_motor_rev_cb(lv_event_t *e)
{
    (void)e;
    /* Release safety lock only for reverse (lowering the arm) */
    safety_lock_set(false);
    motor_reverse_set(true);
    jog_update_motor_label();
    jog_update_lock_label();
}

static void jog_actuator_on_cb(lv_event_t *e)
{
    (void)e;
    release_set(true);
    jog_update_actuator_label();
}

static void jog_actuator_off_cb(lv_event_t *e)
{
    (void)e;
    release_set(false);
    jog_update_actuator_label();
}

static void jog_lock_on_cb(lv_event_t *e)
{
    (void)e;
    safety_lock_set(true);
    jog_update_lock_label();
}

static void jog_lock_off_cb(lv_event_t *e)
{
    (void)e;
    safety_lock_set(false);
    jog_update_lock_label();
}

static void jog_servo_move(float delta)
{
    s_jog_servo_angle += delta;
    if (s_jog_servo_angle < 0.0f)   s_jog_servo_angle = 0.0f;
    if (s_jog_servo_angle > 180.0f) s_jog_servo_angle = 180.0f;
    servo_set_angle(s_jog_servo_angle);
    jog_update_servo_label();
}

static void jog_servo_m10_cb(lv_event_t *e) { (void)e; jog_servo_move(-10.0f); }
static void jog_servo_m1_cb (lv_event_t *e) { (void)e; jog_servo_move( -1.0f); }
static void jog_servo_p1_cb (lv_event_t *e) { (void)e; jog_servo_move(  1.0f); }
static void jog_servo_p10_cb(lv_event_t *e) { (void)e; jog_servo_move( 10.0f); }

static void jog_servo_goto_cb(lv_event_t *e)
{
    float target = (float)(intptr_t)lv_event_get_user_data(e);
    s_jog_servo_angle = target;
    if (s_jog_servo_angle < 0.0f)   s_jog_servo_angle = 0.0f;
    if (s_jog_servo_angle > 180.0f) s_jog_servo_angle = 180.0f;
    servo_jog_to_angle(s_jog_servo_angle, 2000);
    jog_update_servo_label();
}

static void jog_all_stop_cb(lv_event_t *e)
{
    (void)e;
    motor_forward_set(false);
    motor_reverse_set(false);
    release_set(false);
    safety_lock_set(true);
    jog_update_motor_label();
    jog_update_actuator_label();
    jog_update_lock_label();
}

static void jog_endstop_refresh_cb(lv_event_t *e)
{
    (void)e;
    if (s_jog_endstop_lbl) {
        bool pressed = endstop_top_is_pressed();
        lv_label_set_text(s_jog_endstop_lbl, pressed ? "TOP: PRESSED" : "TOP: open");
        lv_obj_set_style_text_color(s_jog_endstop_lbl,
            pressed ? UI_COLOR_WARNING : UI_COLOR_SUCCESS, 0);
    }
    if (s_jog_endstop_bot_lbl) {
        bool pressed = endstop_bot_is_pressed();
        lv_label_set_text(s_jog_endstop_bot_lbl, pressed ? "BOT: PRESSED" : "BOT: open");
        lv_obj_set_style_text_color(s_jog_endstop_bot_lbl,
            pressed ? UI_COLOR_WARNING : UI_COLOR_SUCCESS, 0);
    }
}

static void jog_endstop_timer_cb(lv_timer_t *t)
{
    (void)t;
    jog_endstop_refresh_cb(NULL);
}

/* Helper: create a section card with a title */
static lv_obj_t *jog_make_section(lv_obj_t *parent, const char *title, int w)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    return card;
}

static lv_obj_t *jog_make_btn_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static void create_tab_machine_test(lv_obj_t *parent)
{
    /* Reset handles */
    s_jog_motor_lbl      = NULL;
    s_jog_act_lbl        = NULL;
    s_jog_lock_lbl       = NULL;
    s_jog_servo_lbl      = NULL;
    s_jog_endstop_lbl    = NULL;
    s_jog_endstop_bot_lbl = NULL;
    s_jog_servo_angle = test_manager_get_brake_angle();

    /* ── Title ── */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Machine Test / Jog");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    /* ── Safety warning banner ── */
    lv_obj_t *warn = lv_obj_create(parent);
    lv_obj_set_size(warn, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(warn, lv_color_hex(0x3B1A1A), 0);
    lv_obj_set_style_bg_opa(warn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(warn, UI_COLOR_DANGER, 0);
    lv_obj_set_style_border_width(warn, 1, 0);
    lv_obj_set_style_radius(warn, 6, 0);
    lv_obj_set_style_pad_all(warn, 10, 0);
    lv_obj_clear_flag(warn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *warn_lbl = lv_label_create(warn);
    lv_label_set_text(warn_lbl,
        LV_SYMBOL_WARNING "  All safety interlocks are bypassed here. "
        "Ensure the test area is clear before energising any output.");
    lv_obj_set_style_text_color(warn_lbl, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(warn_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_width(warn_lbl, lv_pct(100));

    /* ── Top row: Motor | Actuator | Endstop ── */
    lv_obj_t *top_row = lv_obj_create(parent);
    lv_obj_set_size(top_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(top_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_row, 0, 0);
    lv_obj_set_style_pad_all(top_row, 0, 0);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(top_row, 12, 0);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(top_row, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Motor card ── */
    lv_obj_t *motor_card = jog_make_section(top_row, LV_SYMBOL_POWER " Motor", 260);
    s_jog_motor_lbl = lv_label_create(motor_card);
    lv_obj_set_style_text_font(s_jog_motor_lbl, &lv_font_montserrat_20, 0);
    jog_update_motor_label();

    lv_obj_t *mrow = jog_make_btn_row(motor_card);

    lv_obj_t *btn_mon = lv_btn_create(mrow);
    lv_obj_add_style(btn_mon, &style_btn_danger, 0);
    lv_obj_set_size(btn_mon,  72, 44);
    lv_obj_add_event_cb(btn_mon, jog_motor_on_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lmon = lv_label_create(btn_mon);
    lv_label_set_text(lmon, LV_SYMBOL_UP " FWD");
    lv_obj_set_style_text_font(lmon, &lv_font_montserrat_18, 0);
    lv_obj_center(lmon);

    lv_obj_t *btn_moff = lv_btn_create(mrow);
    lv_obj_add_style(btn_moff, &style_btn_secondary, 0);
    lv_obj_set_size(btn_moff, 72, 44);
    lv_obj_add_event_cb(btn_moff, jog_motor_off_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lmoff = lv_label_create(btn_moff);
    lv_label_set_text(lmoff, LV_SYMBOL_STOP " STOP");
    lv_obj_set_style_text_font(lmoff, &lv_font_montserrat_18, 0);
    lv_obj_center(lmoff);

    lv_obj_t *btn_mrev = lv_btn_create(mrow);
    lv_obj_add_style(btn_mrev, &style_btn_danger, 0);
    lv_obj_set_size(btn_mrev, 72, 44);
    lv_obj_add_event_cb(btn_mrev, jog_motor_rev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lmrev = lv_label_create(btn_mrev);
    lv_label_set_text(lmrev, LV_SYMBOL_DOWN " REV");
    lv_obj_set_style_text_font(lmrev, &lv_font_montserrat_18, 0);
    lv_obj_center(lmrev);

    /* ── Actuator / Release card ── */
    lv_obj_t *act_card = jog_make_section(top_row, LV_SYMBOL_CHARGE " Release", 200);
    s_jog_act_lbl = lv_label_create(act_card);
    lv_obj_set_style_text_font(s_jog_act_lbl, &lv_font_montserrat_20, 0);
    jog_update_actuator_label();

    lv_obj_t *arow = jog_make_btn_row(act_card);

    lv_obj_t *btn_aon = lv_btn_create(arow);
    lv_obj_add_style(btn_aon, &style_btn_danger, 0);
    lv_obj_set_size(btn_aon,  82, 44);
    lv_obj_add_event_cb(btn_aon, jog_actuator_on_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *laon = lv_label_create(btn_aon);
    lv_label_set_text(laon, LV_SYMBOL_PLAY " ON");
    lv_obj_set_style_text_font(laon, &lv_font_montserrat_18, 0);
    lv_obj_center(laon);

    lv_obj_t *btn_aoff = lv_btn_create(arow);
    lv_obj_add_style(btn_aoff, &style_btn_secondary, 0);
    lv_obj_set_size(btn_aoff, 82, 44);
    lv_obj_add_event_cb(btn_aoff, jog_actuator_off_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *laoff = lv_label_create(btn_aoff);
    lv_label_set_text(laoff, LV_SYMBOL_STOP " OFF");
    lv_obj_set_style_text_font(laoff, &lv_font_montserrat_18, 0);
    lv_obj_center(laoff);

    /* ── Safety Lock card ── */
    lv_obj_t *lock_card = jog_make_section(top_row, LV_SYMBOL_WARNING " Safety Lock", 220);
    s_jog_lock_lbl = lv_label_create(lock_card);
    lv_obj_set_style_text_font(s_jog_lock_lbl, &lv_font_montserrat_20, 0);
    jog_update_lock_label();

    lv_obj_t *lrow = jog_make_btn_row(lock_card);

    lv_obj_t *btn_lon = lv_btn_create(lrow);
    lv_obj_add_style(btn_lon, &style_btn_primary, 0);
    lv_obj_set_size(btn_lon,  92, 44);
    lv_obj_add_event_cb(btn_lon, jog_lock_on_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *llon = lv_label_create(btn_lon);
    lv_label_set_text(llon, LV_SYMBOL_OK " Engage");
    lv_obj_set_style_text_font(llon, &lv_font_montserrat_18, 0);
    lv_obj_center(llon);

    lv_obj_t *btn_loff = lv_btn_create(lrow);
    lv_obj_add_style(btn_loff, &style_btn_danger, 0);
    lv_obj_set_size(btn_loff, 92, 44);
    lv_obj_add_event_cb(btn_loff, jog_lock_off_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lloff = lv_label_create(btn_loff);
    lv_label_set_text(lloff, LV_SYMBOL_CLOSE " Release");
    lv_obj_set_style_text_font(lloff, &lv_font_montserrat_18, 0);
    lv_obj_center(lloff);

    /* ── Endstop card — own row below output controls ── */
    lv_obj_t *es_card = jog_make_section(parent, LV_SYMBOL_EYE_OPEN " Endstops", lv_pct(100));
    s_jog_endstop_lbl = lv_label_create(es_card);
    lv_obj_set_style_text_font(s_jog_endstop_lbl, &lv_font_montserrat_20, 0);
    s_jog_endstop_bot_lbl = lv_label_create(es_card);
    lv_obj_set_style_text_font(s_jog_endstop_bot_lbl, &lv_font_montserrat_20, 0);
    jog_endstop_refresh_cb(NULL);   /* populate immediately */

    /* Auto-refresh endstop labels every 300 ms while this tab is visible */
    s_endstop_poll_timer = lv_timer_create(jog_endstop_timer_cb, 300, NULL);

    lv_obj_t *btn_es_ref = lv_btn_create(es_card);
    lv_obj_add_style(btn_es_ref, &style_btn_secondary, 0);
    lv_obj_set_size(btn_es_ref, 150, 44);
    lv_obj_add_event_cb(btn_es_ref, jog_endstop_refresh_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *les = lv_label_create(btn_es_ref);
    lv_label_set_text(les, LV_SYMBOL_REFRESH " Refresh");
    lv_obj_set_style_text_font(les, &lv_font_montserrat_18, 0);
    lv_obj_center(les);

    /* ── Brake Servo card (full width) ── */
    lv_obj_t *srv_card = jog_make_section(parent, LV_SYMBOL_SETTINGS " Brake Servo", lv_pct(100));

    s_jog_servo_lbl = lv_label_create(srv_card);
    lv_obj_set_style_text_font(s_jog_servo_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_jog_servo_lbl, UI_COLOR_TEXT, 0);
    jog_update_servo_label();

    lv_obj_t *srow = jog_make_btn_row(srv_card);

    /* -10° */
    lv_obj_t *bm10 = lv_btn_create(srow);
    lv_obj_add_style(bm10, &style_btn_secondary, 0);
    lv_obj_set_size(bm10, 84, 48);
    lv_obj_add_event_cb(bm10, jog_servo_m10_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lm10 = lv_label_create(bm10); lv_label_set_text(lm10, "-10\u00b0");
    lv_obj_set_style_text_font(lm10, &lv_font_montserrat_18, 0); lv_obj_center(lm10);

    /* -1° */
    lv_obj_t *bm1 = lv_btn_create(srow);
    lv_obj_add_style(bm1, &style_btn_secondary, 0);
    lv_obj_set_size(bm1, 72, 48);
    lv_obj_add_event_cb(bm1, jog_servo_m1_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lm1 = lv_label_create(bm1); lv_label_set_text(lm1, "-1\u00b0");
    lv_obj_set_style_text_font(lm1, &lv_font_montserrat_18, 0); lv_obj_center(lm1);

    /* +1° */
    lv_obj_t *bp1 = lv_btn_create(srow);
    lv_obj_add_style(bp1, &style_btn_secondary, 0);
    lv_obj_set_size(bp1, 72, 48);
    lv_obj_add_event_cb(bp1, jog_servo_p1_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp1 = lv_label_create(bp1); lv_label_set_text(lp1, "+1\u00b0");
    lv_obj_set_style_text_font(lp1, &lv_font_montserrat_18, 0); lv_obj_center(lp1);

    /* +10° */
    lv_obj_t *bp10 = lv_btn_create(srow);
    lv_obj_add_style(bp10, &style_btn_secondary, 0);
    lv_obj_set_size(bp10, 84, 48);
    lv_obj_add_event_cb(bp10, jog_servo_p10_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp10 = lv_label_create(bp10); lv_label_set_text(lp10, "+10\u00b0");
    lv_obj_set_style_text_font(lp10, &lv_font_montserrat_18, 0); lv_obj_center(lp10);

    /* Spacer */
    lv_obj_t *sp = lv_obj_create(srow);
    lv_obj_set_size(sp, 16, 1);
    lv_obj_set_style_bg_opa(sp, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sp, 0, 0);

    /* Goto presets */
    static const float goto_angles[] = {0.0f, 90.0f, 180.0f};
    static const char *goto_labels[] = {"\u2192 0\u00b0", "\u2192 90\u00b0", "\u2192 180\u00b0"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *bg = lv_btn_create(srow);
        lv_obj_add_style(bg, &style_btn_primary, 0);
        lv_obj_set_size(bg, 96, 48);
        lv_obj_add_event_cb(bg, jog_servo_goto_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(int)goto_angles[i]);
        lv_obj_t *lg = lv_label_create(bg); lv_label_set_text(lg, goto_labels[i]);
        lv_obj_set_style_text_font(lg, &lv_font_montserrat_18, 0); lv_obj_center(lg);
    }

    /* ── ALL STOP button ── */
    lv_obj_t *btn_stop = lv_btn_create(parent);
    lv_obj_add_style(btn_stop, &style_btn_danger, 0);
    lv_obj_set_size(btn_stop, 240, 52);
    lv_obj_add_event_cb(btn_stop, jog_all_stop_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lstop = lv_label_create(btn_stop);
    lv_label_set_text(lstop, LV_SYMBOL_STOP " ALL STOP");
    lv_obj_set_style_text_font(lstop, &lv_font_montserrat_24, 0);
    lv_obj_center(lstop);
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

    /* Clear and recreate content — stop endstop timer and null labels first */
    if (s_endstop_poll_timer) {
        lv_timer_del(s_endstop_poll_timer);
        s_endstop_poll_timer = NULL;
    }
    s_jog_endstop_lbl     = NULL;
    s_jog_endstop_bot_lbl = NULL;
    lv_obj_clean(content_panel);
    lv_obj_set_flex_flow(content_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content_panel, 16, 0);

    switch (tab) {
        case 0: create_tab_pendulum(content_panel);     break;
        case 1: create_tab_calibration(content_panel);  break;
        case 2: create_tab_datetime(content_panel);     break;
        case 3: create_tab_sdcard(content_panel);       break;
        case 4: create_tab_audio(content_panel);        break;
        case 5: create_tab_machine_test(content_panel); break;
        case 6: create_tab_about(content_panel);        break;
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
