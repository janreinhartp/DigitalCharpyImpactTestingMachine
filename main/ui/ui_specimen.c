#include "ui.h"
#include "test_manager.h"
#include <string.h>
#include <stdlib.h>

/* Specimen entry widgets */
lv_obj_t *ui_spec_id_ta       = NULL;
lv_obj_t *ui_spec_operator_ta = NULL;
lv_obj_t *ui_spec_material_dd = NULL;
lv_obj_t *ui_spec_width_ta    = NULL;
lv_obj_t *ui_spec_height_ta   = NULL;
lv_obj_t *ui_spec_length_ta   = NULL;
lv_obj_t *ui_spec_temp_ta     = NULL;
lv_obj_t *ui_spec_notes_ta    = NULL;
lv_obj_t *ui_spec_keyboard    = NULL;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

static void btn_back_cb(lv_event_t *e)    { (void)e; ui_show_dashboard(); }

static void btn_arm_start_cb(lv_event_t *e)
{
    (void)e;
    specimen_info_t spec;
    memset(&spec, 0, sizeof(spec));

    strncpy(spec.specimen_id, lv_textarea_get_text(ui_spec_id_ta), sizeof(spec.specimen_id) - 1);
    strncpy(spec.operator_name, lv_textarea_get_text(ui_spec_operator_ta), sizeof(spec.operator_name) - 1);

    /* Material from dropdown */
    char mat_buf[32];
    lv_dropdown_get_selected_str(ui_spec_material_dd, mat_buf, sizeof(mat_buf));
    strncpy(spec.material, mat_buf, sizeof(spec.material) - 1);

    /* Dimensions */
    spec.width_mm  = (float)atof(lv_textarea_get_text(ui_spec_width_ta));
    spec.height_mm = (float)atof(lv_textarea_get_text(ui_spec_height_ta));
    spec.length_mm = (float)atof(lv_textarea_get_text(ui_spec_length_ta));
    spec.temperature_c = (float)atof(lv_textarea_get_text(ui_spec_temp_ta));

    strncpy(spec.notes, lv_textarea_get_text(ui_spec_notes_ta), sizeof(spec.notes) - 1);

    /* Start test */
    if (test_manager_get_state() == TEST_STATE_IDLE) {
        test_manager_start_new();
    }
    test_manager_arm(&spec);
    ui_show_test_active();
}

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (ui_spec_keyboard == NULL) return;
    lv_keyboard_set_textarea(ui_spec_keyboard, ta);
    lv_obj_remove_flag(ui_spec_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void keyboard_ready_cb(lv_event_t *e)
{
    (void)e;
    if (ui_spec_keyboard) {
        lv_obj_add_flag(ui_spec_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_labeled_ta(lv_obj_t *parent, const char *label_text, const char *placeholder,
                                   int w, int h, bool numeric)
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
    lv_obj_set_size(ta, lv_pct(100), h);
    lv_textarea_set_one_line(ta, h <= 48);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_add_style(ta, &style_input, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    if (numeric) {
        lv_textarea_set_accepted_chars(ta, "0123456789.");
    }

    return ta;
}

void ui_specimen_create(void)
{
    scr_specimen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_specimen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_specimen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_specimen, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_specimen);

    /* Content area */
    lv_obj_t *content = lv_obj_create(scr_specimen);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 12, 0);

    /* Header row */
    lv_obj_t *header = lv_obj_create(content);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "NEW TEST - Enter Specimen Details");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_lbl);

    /* Form row 1: Specimen ID + Operator */
    lv_obj_t *row1 = lv_obj_create(content);
    lv_obj_set_size(row1, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row1, 20, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    ui_spec_id_ta = create_labeled_ta(row1, "Specimen ID", "e.g. STEEL-043", 470, 44, false);
    ui_spec_operator_ta = create_labeled_ta(row1, "Operator", "e.g. J. Reyes", 470, 44, false);

    /* Form row 2: Material + Dimensions */
    lv_obj_t *row2 = lv_obj_create(content);
    lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row2, 20, 0);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    /* Material dropdown */
    lv_obj_t *mat_cont = lv_obj_create(row2);
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

    ui_spec_material_dd = lv_dropdown_create(mat_cont);
    lv_dropdown_set_options(ui_spec_material_dd,
        "Mild Steel\nStainless Steel\nAluminum\nCopper\nPlastic\nCustom");
    lv_obj_set_width(ui_spec_material_dd, lv_pct(100));
    lv_obj_set_style_bg_color(ui_spec_material_dd, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(ui_spec_material_dd, UI_COLOR_BORDER, 0);
    lv_obj_set_style_text_color(ui_spec_material_dd, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_spec_material_dd, &lv_font_montserrat_20, 0);

    /* Dimensions container: W × H × L */
    lv_obj_t *dim_cont = lv_obj_create(row2);
    lv_obj_set_size(dim_cont, 470, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(dim_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dim_cont, 0, 0);
    lv_obj_set_style_pad_all(dim_cont, 0, 0);
    lv_obj_set_flex_flow(dim_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(dim_cont, 4, 0);
    lv_obj_clear_flag(dim_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dim_lbl = lv_label_create(dim_cont);
    lv_label_set_text(dim_lbl, "Dimensions WxHxL (mm)");
    lv_obj_set_style_text_font(dim_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dim_lbl, UI_COLOR_TEXT_SEC, 0);

    lv_obj_t *dim_row = lv_obj_create(dim_cont);
    lv_obj_set_size(dim_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(dim_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dim_row, 0, 0);
    lv_obj_set_style_pad_all(dim_row, 0, 0);
    lv_obj_set_flex_flow(dim_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dim_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dim_row, 8, 0);
    lv_obj_clear_flag(dim_row, LV_OBJ_FLAG_SCROLLABLE);

    ui_spec_width_ta = lv_textarea_create(dim_row);
    lv_obj_set_size(ui_spec_width_ta, 120, 44);
    lv_textarea_set_one_line(ui_spec_width_ta, true);
    lv_textarea_set_text(ui_spec_width_ta, "10");
    lv_textarea_set_accepted_chars(ui_spec_width_ta, "0123456789.");
    lv_obj_add_style(ui_spec_width_ta, &style_input, 0);
    lv_obj_add_event_cb(ui_spec_width_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *sep1 = lv_label_create(dim_row);
    lv_label_set_text(sep1, "x");
    lv_obj_set_style_text_color(sep1, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sep1, &lv_font_montserrat_20, 0);

    ui_spec_height_ta = lv_textarea_create(dim_row);
    lv_obj_set_size(ui_spec_height_ta, 120, 44);
    lv_textarea_set_one_line(ui_spec_height_ta, true);
    lv_textarea_set_text(ui_spec_height_ta, "10");
    lv_textarea_set_accepted_chars(ui_spec_height_ta, "0123456789.");
    lv_obj_add_style(ui_spec_height_ta, &style_input, 0);
    lv_obj_add_event_cb(ui_spec_height_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *sep2 = lv_label_create(dim_row);
    lv_label_set_text(sep2, "x");
    lv_obj_set_style_text_color(sep2, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sep2, &lv_font_montserrat_20, 0);

    ui_spec_length_ta = lv_textarea_create(dim_row);
    lv_obj_set_size(ui_spec_length_ta, 120, 44);
    lv_textarea_set_one_line(ui_spec_length_ta, true);
    lv_textarea_set_text(ui_spec_length_ta, "55");
    lv_textarea_set_accepted_chars(ui_spec_length_ta, "0123456789.");
    lv_obj_add_style(ui_spec_length_ta, &style_input, 0);
    lv_obj_add_event_cb(ui_spec_length_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    /* Form row 3: Temperature + Notes */
    lv_obj_t *row3 = lv_obj_create(content);
    lv_obj_set_size(row3, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row3, 20, 0);
    lv_obj_clear_flag(row3, LV_OBJ_FLAG_SCROLLABLE);

    ui_spec_temp_ta = create_labeled_ta(row3, "Temperature (\u00b0C)", "e.g. 23", 220, 44, true);
    ui_spec_notes_ta = create_labeled_ta(row3, "Notes (optional)", "", 720, 44, false);

    /* ARM & START button */
    lv_obj_t *btn_arm = lv_btn_create(content);
    lv_obj_add_style(btn_arm, &style_btn_primary, 0);
    lv_obj_set_size(btn_arm, lv_pct(100), 56);
    lv_obj_add_event_cb(btn_arm, btn_arm_start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arm_lbl = lv_label_create(btn_arm);
    lv_label_set_text(arm_lbl, LV_SYMBOL_PLAY " ARM & START TEST");
    lv_obj_set_style_text_font(arm_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(arm_lbl);

    /* On-screen keyboard (hidden initially) */
    ui_spec_keyboard = lv_keyboard_create(scr_specimen);
    lv_obj_set_size(ui_spec_keyboard, 1024, 220);
    lv_obj_align(ui_spec_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(ui_spec_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ui_spec_keyboard, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_color(ui_spec_keyboard, UI_COLOR_SURFACE_EL, LV_PART_ITEMS);
    lv_obj_set_style_text_color(ui_spec_keyboard, UI_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_add_event_cb(ui_spec_keyboard, keyboard_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_spec_keyboard, keyboard_ready_cb, LV_EVENT_CANCEL, NULL);
}
