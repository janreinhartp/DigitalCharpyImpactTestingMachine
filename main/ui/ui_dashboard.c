#include "ui.h"

/* Dashboard widgets */
lv_obj_t *ui_dash_arc = NULL;
lv_obj_t *ui_dash_angle_label = NULL;
lv_obj_t *ui_dash_last_angle_label = NULL;
lv_obj_t *ui_dash_last_energy_label = NULL;
lv_obj_t *ui_dash_last_specimen_label = NULL;
lv_obj_t *ui_dash_last_material_label = NULL;
lv_obj_t *ui_dash_last_operator_label = NULL;
lv_obj_t *ui_dash_last_timestamp_label = NULL;

/* Forward declarations for external status bar factory */
extern void ui_create_screen_status_bar(lv_obj_t *screen);

static void btn_new_test_cb(lv_event_t *e) { (void)e; ui_show_specimen(); }
static void btn_history_cb(lv_event_t *e)  { (void)e; ui_show_history(); }
static void btn_settings_cb(lv_event_t *e) { (void)e; ui_show_settings(); }
static void btn_dbtt_cb(lv_event_t *e)     { (void)e; ui_show_dbtt(); }

void ui_dashboard_create(void)
{
    scr_dashboard = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dashboard, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_dashboard, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_dashboard, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_dashboard);

    /* ——— Content area below status bar ——— */
    lv_obj_t *content = lv_obj_create(scr_dashboard);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ——— Left half: Arc gauge (500px) ——— */
    lv_obj_t *left = lv_obj_create(content);
    lv_obj_set_size(left, 500, 480);
    lv_obj_set_pos(left, 0, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    ui_dash_arc = lv_arc_create(left);
    lv_obj_set_size(ui_dash_arc, 320, 320);
    lv_obj_center(ui_dash_arc);
    lv_arc_set_rotation(ui_dash_arc, 135);
    lv_arc_set_bg_angles(ui_dash_arc, 0, 270);
    lv_arc_set_range(ui_dash_arc, 0, 360);
    lv_arc_set_value(ui_dash_arc, 0);
    lv_obj_remove_flag(ui_dash_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(ui_dash_arc, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_dash_arc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_dash_arc, UI_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_dash_arc, 16, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui_dash_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    /* Center label inside arc */
    ui_dash_angle_label = lv_label_create(ui_dash_arc);
    lv_label_set_text(ui_dash_angle_label, "0.0°");
    lv_obj_set_style_text_font(ui_dash_angle_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_dash_angle_label, UI_COLOR_TEXT, 0);
    lv_obj_center(ui_dash_angle_label);

    /* "LIVE ANGLE" sub-label */
    lv_obj_t *sub_label = lv_label_create(left);
    lv_label_set_text(sub_label, "LIVE ANGLE");
    lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sub_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_align(sub_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* ——— Right half: Last test result card (524px) ——— */
    lv_obj_t *right_card = lv_obj_create(content);
    lv_obj_set_size(right_card, 504, 380);
    lv_obj_set_pos(right_card, 510, 10);
    lv_obj_add_style(right_card, &style_card, 0);
    lv_obj_set_flex_flow(right_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right_card, 8, 0);
    lv_obj_clear_flag(right_card, LV_OBJ_FLAG_SCROLLABLE);

    /* Header row */
    lv_obj_t *header_row = lv_obj_create(right_card);
    lv_obj_set_size(header_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 0, 0);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr = lv_label_create(header_row);
    lv_label_set_text(hdr, "LAST TEST RESULT");
    lv_obj_set_style_text_color(hdr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_20, 0);

    ui_dash_last_timestamp_label = lv_label_create(header_row);
    lv_label_set_text(ui_dash_last_timestamp_label, "--");
    lv_obj_set_style_text_color(ui_dash_last_timestamp_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_dash_last_timestamp_label, &lv_font_montserrat_16, 0);

    /* Result boxes row */
    lv_obj_t *result_row = lv_obj_create(right_card);
    lv_obj_set_size(result_row, lv_pct(100), 120);
    lv_obj_set_style_bg_opa(result_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(result_row, 0, 0);
    lv_obj_set_style_pad_all(result_row, 0, 0);
    lv_obj_set_flex_flow(result_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(result_row, 16, 0);
    lv_obj_clear_flag(result_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Angle box */
    lv_obj_t *angle_box = lv_obj_create(result_row);
    lv_obj_set_size(angle_box, 200, 100);
    lv_obj_set_style_bg_color(angle_box, UI_COLOR_SURFACE_EL, 0);
    lv_obj_set_style_bg_opa(angle_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(angle_box, 8, 0);
    lv_obj_set_style_border_width(angle_box, 0, 0);
    lv_obj_clear_flag(angle_box, LV_OBJ_FLAG_SCROLLABLE);

    ui_dash_last_angle_label = lv_label_create(angle_box);
    lv_label_set_text(ui_dash_last_angle_label, "--°");
    lv_obj_set_style_text_font(ui_dash_last_angle_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_dash_last_angle_label, UI_COLOR_PRIMARY, 0);
    lv_obj_center(ui_dash_last_angle_label);

    /* Energy box */
    lv_obj_t *energy_box = lv_obj_create(result_row);
    lv_obj_set_size(energy_box, 200, 100);
    lv_obj_set_style_bg_color(energy_box, UI_COLOR_SURFACE_EL, 0);
    lv_obj_set_style_bg_opa(energy_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(energy_box, 8, 0);
    lv_obj_set_style_border_width(energy_box, 0, 0);
    lv_obj_clear_flag(energy_box, LV_OBJ_FLAG_SCROLLABLE);

    ui_dash_last_energy_label = lv_label_create(energy_box);
    lv_label_set_text(ui_dash_last_energy_label, "-- J");
    lv_obj_set_style_text_font(ui_dash_last_energy_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_dash_last_energy_label, UI_COLOR_SUCCESS, 0);
    lv_obj_center(ui_dash_last_energy_label);

    /* Specimen info labels */
    ui_dash_last_specimen_label = lv_label_create(right_card);
    lv_label_set_text(ui_dash_last_specimen_label, "Specimen: --");
    lv_obj_set_style_text_color(ui_dash_last_specimen_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_dash_last_specimen_label, &lv_font_montserrat_16, 0);

    ui_dash_last_material_label = lv_label_create(right_card);
    lv_label_set_text(ui_dash_last_material_label, "Material: --");
    lv_obj_set_style_text_color(ui_dash_last_material_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_dash_last_material_label, &lv_font_montserrat_16, 0);

    ui_dash_last_operator_label = lv_label_create(right_card);
    lv_label_set_text(ui_dash_last_operator_label, "Operator: --");
    lv_obj_set_style_text_color(ui_dash_last_operator_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_dash_last_operator_label, &lv_font_montserrat_16, 0);

    /* ——— Bottom nav buttons (80px tall) ——— */
    lv_obj_t *nav = lv_obj_create(content);
    lv_obj_set_size(nav, 1024, 80);
    lv_obj_set_pos(nav, 0, 480);
    lv_obj_set_style_bg_opa(nav, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_pad_hor(nav, 20, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    /* NEW TEST button */
    lv_obj_t *btn_new = lv_btn_create(nav);
    lv_obj_add_style(btn_new, &style_btn_primary, 0);
    lv_obj_set_size(btn_new, 230, 56);
    lv_obj_add_event_cb(btn_new, btn_new_test_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_new = lv_label_create(btn_new);
    lv_label_set_text(lbl_new, LV_SYMBOL_PLAY " NEW TEST");
    lv_obj_set_style_text_font(lbl_new, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_new);

    /* HISTORY button */
    lv_obj_t *btn_hist = lv_btn_create(nav);
    lv_obj_add_style(btn_hist, &style_btn_secondary, 0);
    lv_obj_set_size(btn_hist, 200, 56);
    lv_obj_add_event_cb(btn_hist, btn_history_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_hist = lv_label_create(btn_hist);
    lv_label_set_text(lbl_hist, LV_SYMBOL_LIST " HISTORY");
    lv_obj_set_style_text_font(lbl_hist, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_hist);

    /* DBTT button */
    lv_obj_t *btn_dbtt = lv_btn_create(nav);
    lv_obj_add_style(btn_dbtt, &style_btn_secondary, 0);
    lv_obj_set_size(btn_dbtt, 200, 56);
    lv_obj_add_event_cb(btn_dbtt, btn_dbtt_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_dbtt = lv_label_create(btn_dbtt);
    lv_label_set_text(lbl_dbtt, "DBTT");
    lv_obj_set_style_text_font(lbl_dbtt, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_dbtt);

    /* SETTINGS button */
    lv_obj_t *btn_set = lv_btn_create(nav);
    lv_obj_add_style(btn_set, &style_btn_secondary, 0);
    lv_obj_set_size(btn_set, 200, 56);
    lv_obj_set_style_text_color(btn_set, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_border_color(btn_set, UI_COLOR_TEXT_SEC, 0);
    lv_obj_add_event_cb(btn_set, btn_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_set, LV_SYMBOL_SETTINGS " SETTINGS");
    lv_obj_set_style_text_font(lbl_set, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_set);
}
