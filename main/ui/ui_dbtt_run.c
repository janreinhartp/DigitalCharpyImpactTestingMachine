/*
 * ui_dbtt_run.c — DBTT Per-Test Screen.
 *
 * Shown once for each test in the DBTT session.  The operator enters the
 * specimen temperature, then presses ARM & START to run a standard Charpy
 * test via the existing test_manager / scr_test_active flow.
 *
 * Navigation:
 *   ARM & START → scr_test_active  (save/discard callbacks route back here
 *                                   or to scr_dbtt_result when complete)
 *   Abort Session → resets dbtt_manager, returns to Dashboard
 */

#include "ui.h"
#include "dbtt_manager.h"
#include "test_manager.h"
#include <string.h>
#include <stdlib.h>

/* ——————————————— Screen + dynamic labels ——————————————— */

extern lv_obj_t *scr_dbtt_run;

static lv_obj_t *s_progress_label  = NULL;  /* "Test X of N" */
static lv_obj_t *s_info_label      = NULL;  /* "Material | Operator" */
static lv_obj_t *s_spec_id_label   = NULL;  /* auto specimen ID */
static lv_obj_t *s_temp_ta         = NULL;  /* temperature input */
static lv_obj_t *s_kb              = NULL;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

/* ——————————————— Callbacks ——————————————— */

static void btn_abort_cb(lv_event_t *e)
{
    (void)e;
    dbtt_manager_reset();
    ui_show_dashboard();
}

static void btn_arm_cb(lv_event_t *e)
{
    (void)e;
    const dbtt_session_t *sess = dbtt_manager_get_session();

    specimen_info_t spec;
    memset(&spec, 0, sizeof(spec));

    /* Auto-generated ID: DBTT-{n} */
    snprintf(spec.specimen_id, sizeof(spec.specimen_id),
             "DBTT-%d", sess->n_done + 1);

    strncpy(spec.material,      sess->material,      sizeof(spec.material) - 1);
    strncpy(spec.operator_name, sess->operator_name, sizeof(spec.operator_name) - 1);
    spec.width_mm      = sess->width_mm;
    spec.height_mm     = sess->height_mm;
    spec.length_mm     = sess->length_mm;
    spec.temperature_c = (float)atof(lv_textarea_get_text(s_temp_ta));

    if (test_manager_get_state() == TEST_STATE_IDLE) {
        test_manager_start_new();
    }
    test_manager_arm(&spec);
    ui_show_test_active();
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

/* ——————————————— Public refresh (called from ui_show_dbtt_run) ——————————————— */

void ui_dbtt_run_refresh(void)
{
    const dbtt_session_t *sess = dbtt_manager_get_session();

    char buf[96];

    snprintf(buf, sizeof(buf), "Test %d of %d",
             sess->n_done + 1, sess->n_planned);
    lv_label_set_text(s_progress_label, buf);

    snprintf(buf, sizeof(buf), "%s  |  %s",
             sess->material, sess->operator_name);
    lv_label_set_text(s_info_label, buf);

    snprintf(buf, sizeof(buf), "DBTT-%d", sess->n_done + 1);
    lv_label_set_text(s_spec_id_label, buf);

    lv_textarea_set_text(s_temp_ta, "");
}

/* ——————————————— Screen constructor ——————————————— */

void ui_dbtt_run_create(void)
{
    scr_dbtt_run = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dbtt_run, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_dbtt_run, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_dbtt_run, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_dbtt_run);

    /* ——— Content area ——— */
    lv_obj_t *content = lv_obj_create(scr_dbtt_run);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 24, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 16, 0);
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

    /* Title column */
    lv_obj_t *title_col = lv_obj_create(header);
    lv_obj_set_size(title_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_col, 0, 0);
    lv_obj_set_style_pad_all(title_col, 0, 0);
    lv_obj_set_flex_flow(title_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(title_col, 4, 0);
    lv_obj_clear_flag(title_col, LV_OBJ_FLAG_SCROLLABLE);

    s_progress_label = lv_label_create(title_col);
    lv_label_set_text(s_progress_label, "Test 1 of 5");
    lv_obj_set_style_text_font(s_progress_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_progress_label, UI_COLOR_TEXT, 0);

    s_info_label = lv_label_create(title_col);
    lv_label_set_text(s_info_label, "Material  |  Operator");
    lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_info_label, UI_COLOR_TEXT_SEC, 0);

    /* Abort button */
    lv_obj_t *btn_abort = lv_btn_create(header);
    lv_obj_add_style(btn_abort, &style_btn_danger, 0);
    lv_obj_set_size(btn_abort, 190, 44);
    lv_obj_add_event_cb(btn_abort, btn_abort_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *abort_lbl = lv_label_create(btn_abort);
    lv_label_set_text(abort_lbl, LV_SYMBOL_CLOSE " Abort Session");
    lv_obj_set_style_text_font(abort_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(abort_lbl);

    /* ——— Specimen ID row ——— */
    lv_obj_t *spec_row = lv_obj_create(content);
    lv_obj_set_size(spec_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(spec_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spec_row, 0, 0);
    lv_obj_set_style_pad_all(spec_row, 0, 0);
    lv_obj_set_flex_flow(spec_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(spec_row, 8, 0);
    lv_obj_clear_flag(spec_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *spec_lbl = lv_label_create(spec_row);
    lv_label_set_text(spec_lbl, "Specimen ID:");
    lv_obj_set_style_text_font(spec_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(spec_lbl, UI_COLOR_TEXT_SEC, 0);

    s_spec_id_label = lv_label_create(spec_row);
    lv_label_set_text(s_spec_id_label, "DBTT-1");
    lv_obj_set_style_text_font(s_spec_id_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_spec_id_label, UI_COLOR_PRIMARY, 0);

    /* ——— Temperature input card ——— */
    lv_obj_t *temp_card = lv_obj_create(content);
    lv_obj_set_size(temp_card, 560, LV_SIZE_CONTENT);
    lv_obj_add_style(temp_card, &style_card, 0);
    lv_obj_set_flex_flow(temp_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(temp_card, 10, 0);
    lv_obj_clear_flag(temp_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *temp_title = lv_label_create(temp_card);
    lv_label_set_text(temp_title, "Specimen Temperature (\xc2\xb0""C)");
    lv_obj_set_style_text_font(temp_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(temp_title, UI_COLOR_TEXT_SEC, 0);

    s_temp_ta = lv_textarea_create(temp_card);
    lv_obj_set_size(s_temp_ta, lv_pct(100), 64);
    lv_textarea_set_one_line(s_temp_ta, true);
    lv_textarea_set_placeholder_text(s_temp_ta, "e.g.  -40  or  25");
    lv_obj_add_style(s_temp_ta, &style_input, 0);
    lv_obj_set_style_text_font(s_temp_ta, &lv_font_montserrat_48, 0);
    lv_obj_add_event_cb(s_temp_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *temp_hint = lv_label_create(temp_card);
    lv_label_set_text(temp_hint,
        "Condition the specimen to this temperature before running the test.");
    lv_obj_set_style_text_font(temp_hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(temp_hint, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_width(temp_hint, lv_pct(100));

    /* ——— ARM & START button ——— */
    lv_obj_t *btn_arm = lv_btn_create(content);
    lv_obj_set_size(btn_arm, 380, 64);
    lv_obj_set_style_bg_color(btn_arm, UI_COLOR_DANGER, 0);
    lv_obj_set_style_bg_opa(btn_arm, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_arm, 8, 0);
    lv_obj_add_event_cb(btn_arm, btn_arm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arm_lbl = lv_label_create(btn_arm);
    lv_label_set_text(arm_lbl, LV_SYMBOL_UPLOAD " ARM & START TEST");
    lv_obj_set_style_text_font(arm_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(arm_lbl, UI_COLOR_TEXT, 0);
    lv_obj_center(arm_lbl);

    /* ——— Keyboard ——— */
    s_kb = lv_keyboard_create(scr_dbtt_run);
    lv_obj_set_size(s_kb, 1024, 280);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s_kb, kb_ready_cb, LV_EVENT_READY,  NULL);
    lv_obj_add_event_cb(s_kb, kb_ready_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}
