/*
 * ui_dbtt.c — DBTT Session Setup Screen.
 *
 * Collects: material, operator, number of planned tests, specimen dimensions.
 * On START SESSION: calls dbtt_manager_start() and navigates to scr_dbtt_run.
 * Each individual test in the session will ask for a temperature before arming.
 */

#include "ui.h"
#include "dbtt_manager.h"
#include <stdlib.h>
#include <string.h>

/* ——————————————— Widget handles (static — not needed outside this file) ——————————————— */

/* scr_dbtt_setup is defined (and owned) in ui.c — just reference it here */
extern lv_obj_t *scr_dbtt_setup;

static lv_obj_t *s_material_dd  = NULL;
static lv_obj_t *s_operator_ta  = NULL;
static lv_obj_t *s_n_ta         = NULL;
static lv_obj_t *s_width_ta     = NULL;
static lv_obj_t *s_height_ta    = NULL;
static lv_obj_t *s_length_ta    = NULL;
static lv_obj_t *s_kb           = NULL;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

/* ——————————————— Callbacks ——————————————— */

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    ui_show_dashboard();
}

static void btn_history_cb(lv_event_t *e)
{
    (void)e;
    ui_show_dbtt_history();
}

static void btn_start_cb(lv_event_t *e)
{
    (void)e;

    char mat_buf[32];
    lv_dropdown_get_selected_str(s_material_dd, mat_buf, sizeof(mat_buf));

    const char *op = lv_textarea_get_text(s_operator_ta);

    int n = atoi(lv_textarea_get_text(s_n_ta));
    if (n < 1)  n = 1;
    if (n > DBTT_SESSION_MAX_TESTS) n = DBTT_SESSION_MAX_TESTS;

    float w = (float)atof(lv_textarea_get_text(s_width_ta));
    float h = (float)atof(lv_textarea_get_text(s_height_ta));
    float l = (float)atof(lv_textarea_get_text(s_length_ta));

    dbtt_manager_start(mat_buf, op, n, w, h, l);
    ui_show_dbtt_run();
}

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (s_kb) {
        lv_keyboard_set_textarea(s_kb, ta);
        lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void kb_ready_cb(lv_event_t *e)
{
    (void)e;
    if (s_kb) lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

/* ——————————————— Helper: labeled text-area field ——————————————— */

static lv_obj_t *make_field(lv_obj_t *parent, const char *label_text,
                             const char *placeholder, int w,
                             const char *default_val, bool numeric)
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
    lv_textarea_set_placeholder_text(ta, placeholder);
    if (default_val) lv_textarea_set_text(ta, default_val);
    lv_obj_add_style(ta, &style_input, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    if (numeric) lv_textarea_set_accepted_chars(ta, "0123456789.");
    return ta;
}

/* ——————————————— Screen constructor ——————————————— */

void ui_dbtt_setup_create(void)
{
    scr_dbtt_setup = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dbtt_setup, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_dbtt_setup, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_dbtt_setup, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_dbtt_setup);

    /* ——— Content area ——— */
    lv_obj_t *content = lv_obj_create(scr_dbtt_setup);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 18, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ——— Header ——— */
    lv_obj_t *header = lv_obj_create(content);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "DBTT - Session Setup");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(back_lbl);

    /* ——— Row 1: Material dropdown + Operator ——— */
    lv_obj_t *row1 = lv_obj_create(content);
    lv_obj_set_size(row1, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row1, 20, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    /* Material container */
    lv_obj_t *mat_cont = lv_obj_create(row1);
    lv_obj_set_size(mat_cont, 470, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mat_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mat_cont, 0, 0);
    lv_obj_set_style_pad_all(mat_cont, 0, 0);
    lv_obj_set_flex_flow(mat_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mat_cont, 4, 0);
    lv_obj_clear_flag(mat_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mat_lbl = lv_label_create(mat_cont);
    lv_label_set_text(mat_lbl, "Material Type");
    lv_obj_set_style_text_font(mat_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mat_lbl, UI_COLOR_TEXT_SEC, 0);

    s_material_dd = lv_dropdown_create(mat_cont);
    lv_dropdown_set_options(s_material_dd,
        "Mild Steel\nStainless Steel\nAluminum\nCopper\nPlastic\nCustom");
    lv_obj_set_width(s_material_dd, lv_pct(100));
    lv_obj_set_style_bg_color(s_material_dd, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(s_material_dd, UI_COLOR_BORDER, 0);
    lv_obj_set_style_text_color(s_material_dd, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_material_dd, &lv_font_montserrat_20, 0);

    s_operator_ta = make_field(row1, "Operator", "e.g. J. Reyes", 470, "", false);

    /* ——— Row 2: N tests + Dimensions ——— */
    lv_obj_t *row2 = lv_obj_create(content);
    lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row2, 20, 0);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    s_n_ta      = make_field(row2, "Number of Tests (1-20)", "5",  190, "5",  true);
    s_width_ta  = make_field(row2, "Width (mm)",  "10", 190, "10", true);
    s_height_ta = make_field(row2, "Height (mm)", "10", 190, "10", true);
    s_length_ta = make_field(row2, "Length (mm)", "55", 190, "55", true);

    /* ——— Description ——— */
    lv_obj_t *desc = lv_label_create(content);
    lv_label_set_text(desc,
        "Each test will prompt for a specimen temperature before arming the pendulum.\n"
        "Results are plotted as Absorbed Energy (J) vs Temperature (\xc2\xb0""C) to find the transition point.");
    lv_obj_set_style_text_color(desc, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_16, 0);
    lv_obj_set_width(desc, lv_pct(100));

    /* ——— Button row: START SESSION + View History ——— */
    lv_obj_t *btn_row = lv_obj_create(content);
    lv_obj_set_size(btn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 16, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_start = lv_btn_create(btn_row);
    lv_obj_add_style(btn_start, &style_btn_primary, 0);
    lv_obj_set_size(btn_start, 320, 56);
    lv_obj_add_event_cb(btn_start, btn_start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_lbl = lv_label_create(btn_start);
    lv_label_set_text(start_lbl, LV_SYMBOL_PLAY " START SESSION");
    lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(start_lbl);

    lv_obj_t *btn_hist = lv_btn_create(btn_row);
    lv_obj_add_style(btn_hist, &style_btn_secondary, 0);
    lv_obj_set_size(btn_hist, 200, 56);
    lv_obj_add_event_cb(btn_hist, btn_history_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *hist_lbl = lv_label_create(btn_hist);
    lv_label_set_text(hist_lbl, LV_SYMBOL_LIST " History");
    lv_obj_set_style_text_font(hist_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(hist_lbl);

    /* ——— Keyboard (shared within this screen) ——— */
    s_kb = lv_keyboard_create(scr_dbtt_setup);
    lv_obj_set_size(s_kb, 1024, 280);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s_kb, kb_ready_cb, LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(s_kb, kb_ready_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}
