#include "ui.h"
#include "test_manager.h"
#include "dbtt_manager.h"

/* Test active widgets */
lv_obj_t *ui_test_arc = NULL;
lv_obj_t *ui_test_angle_label = NULL;
lv_obj_t *ui_test_state_label = NULL;
lv_obj_t *ui_test_detail_label = NULL;
lv_obj_t *ui_test_progress_dots[4] = {NULL};
lv_obj_t *ui_test_result_angle_label = NULL;
lv_obj_t *ui_test_result_energy_label = NULL;
lv_obj_t *ui_test_btn_release = NULL;
lv_obj_t *ui_test_btn_abort = NULL;
lv_obj_t *ui_test_btn_save = NULL;
lv_obj_t *ui_test_btn_save_void = NULL;
lv_obj_t *ui_test_btn_discard = NULL;
lv_obj_t *ui_test_btn_retry = NULL;
lv_obj_t *ui_test_result_panel = NULL;

static lv_obj_t *action_area = NULL;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

static void btn_release_cb(lv_event_t *e) { (void)e; test_manager_release(); }
static void btn_abort_cb(lv_event_t *e)   { (void)e; test_manager_abort(); ui_show_dashboard(); }

/* "DONE" / "DISCARD" — navigate away, result was already auto-saved (or voided) */
static void btn_done_cb(lv_event_t *e)
{
    (void)e;
    const dbtt_session_t *sess = dbtt_manager_get_session();
    bool dbtt_all_done = (!dbtt_manager_is_active() &&
                          sess->n_done > 0 &&
                          sess->n_done >= sess->n_planned);
    bool dbtt_more = dbtt_manager_is_active();

    test_manager_discard_result();   /* transition state machine back to IDLE */

    if (dbtt_all_done) {
        ui_show_dbtt_result();       /* all DBTT tests done — show curve */
    } else if (dbtt_more) {
        ui_show_dbtt_run();          /* next DBTT test */
    } else {
        ui_show_dashboard();
    }
}

/* Retry the same specimen after a void (not-cut) result */
static void btn_retry_cb(lv_event_t *e)
{
    (void)e;
    test_manager_retry();   /* COMPLETE+void → HOMING, keeps current specimen */
}

/* Save a void (not-cut) result as a valid data point then navigate */
static void btn_save_void_cb(lv_event_t *e)
{
    (void)e;
    const dbtt_session_t *sess = dbtt_manager_get_session();
    bool dbtt_was_active = dbtt_manager_is_active();

    test_manager_save_result();

    /* Session may have completed inside save — check both before and after */
    bool dbtt_all_done = (dbtt_was_active && !dbtt_manager_is_active() &&
                          sess->n_done >= sess->n_planned);
    bool dbtt_more = dbtt_manager_is_active();

    if (dbtt_all_done)  ui_show_dbtt_result();
    else if (dbtt_more) ui_show_dbtt_run();
    else                ui_show_dashboard();
}

void ui_test_active_create(void)
{
    scr_test_active = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_test_active, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_test_active, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_test_active, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_test_active);

    /* Content area */
    lv_obj_t *content = lv_obj_create(scr_test_active);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ——— Large arc gauge (center, dominant) ——— */
    ui_test_arc = lv_arc_create(content);
    lv_obj_set_size(ui_test_arc, 300, 300);
    lv_obj_align(ui_test_arc, LV_ALIGN_TOP_MID, 0, 10);
    lv_arc_set_rotation(ui_test_arc, 135);
    lv_arc_set_bg_angles(ui_test_arc, 0, 270);
    lv_arc_set_range(ui_test_arc, 0, 360);
    lv_arc_set_value(ui_test_arc, 0);
    lv_obj_remove_flag(ui_test_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(ui_test_arc, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_test_arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_test_arc, UI_COLOR_WARNING, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_test_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui_test_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    ui_test_angle_label = lv_label_create(ui_test_arc);
    lv_label_set_text(ui_test_angle_label, "0.0°");
    lv_obj_set_style_text_font(ui_test_angle_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_test_angle_label, UI_COLOR_TEXT, 0);
    lv_obj_center(ui_test_angle_label);

    /* ——— Progress dots + state text ——— */
    lv_obj_t *state_bar = lv_obj_create(content);
    lv_obj_set_size(state_bar, 900, 50);
    lv_obj_align(state_bar, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_set_style_bg_color(state_bar, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(state_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state_bar, 8, 0);
    lv_obj_set_style_border_width(state_bar, 0, 0);
    lv_obj_set_flex_flow(state_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(state_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(state_bar, 16, 0);
    lv_obj_set_style_pad_column(state_bar, 8, 0);
    lv_obj_clear_flag(state_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 4 progress dots */
    for (int i = 0; i < 4; i++) {
        ui_test_progress_dots[i] = lv_obj_create(state_bar);
        lv_obj_set_size(ui_test_progress_dots[i], 14, 14);
        lv_obj_set_style_radius(ui_test_progress_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(ui_test_progress_dots[i], UI_COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_bg_opa(ui_test_progress_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ui_test_progress_dots[i], 0, 0);
        lv_obj_clear_flag(ui_test_progress_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    ui_test_state_label = lv_label_create(state_bar);
    lv_label_set_text(ui_test_state_label, "ARMING - Lifting arm... Stand clear.");
    lv_obj_set_style_text_color(ui_test_state_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_test_state_label, &lv_font_montserrat_20, 0);

    /* ——— Detail / sub-step label (below state bar) ——— */
    lv_obj_t *detail_cont = lv_obj_create(content);
    lv_obj_set_size(detail_cont, 900, 32);
    lv_obj_align(detail_cont, LV_ALIGN_TOP_MID, 0, 374);
    lv_obj_set_style_bg_opa(detail_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(detail_cont, 0, 0);
    lv_obj_set_style_pad_all(detail_cont, 0, 0);
    lv_obj_clear_flag(detail_cont, LV_OBJ_FLAG_SCROLLABLE);

    ui_test_detail_label = lv_label_create(detail_cont);
    lv_label_set_text(ui_test_detail_label, "");
    lv_obj_set_style_text_font(ui_test_detail_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_test_detail_label, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_align(ui_test_detail_label, LV_ALIGN_LEFT_MID, 4, 0);

    /* ——— Result panel (hidden until COMPLETE) ——— */
    ui_test_result_panel = lv_obj_create(content);
    lv_obj_set_size(ui_test_result_panel, 900, 70);
    lv_obj_align(ui_test_result_panel, LV_ALIGN_TOP_MID, 0, 380);
    lv_obj_set_style_bg_opa(ui_test_result_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_test_result_panel, 0, 0);
    lv_obj_set_style_pad_all(ui_test_result_panel, 0, 0);
    lv_obj_set_flex_flow(ui_test_result_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(ui_test_result_panel, 16, 0);
    lv_obj_clear_flag(ui_test_result_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_test_result_panel, LV_OBJ_FLAG_HIDDEN);

    /* Angle result card */
    lv_obj_t *r_angle = lv_obj_create(ui_test_result_panel);
    lv_obj_set_size(r_angle, 240, 60);
    lv_obj_set_style_bg_color(r_angle, UI_COLOR_SURFACE_EL, 0);
    lv_obj_set_style_bg_opa(r_angle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(r_angle, 8, 0);
    lv_obj_set_style_border_width(r_angle, 0, 0);
    lv_obj_clear_flag(r_angle, LV_OBJ_FLAG_SCROLLABLE);

    ui_test_result_angle_label = lv_label_create(r_angle);
    lv_label_set_text(ui_test_result_angle_label, "--°");
    lv_obj_set_style_text_font(ui_test_result_angle_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(ui_test_result_angle_label, UI_COLOR_PRIMARY, 0);
    lv_obj_center(ui_test_result_angle_label);

    /* Energy result card */
    lv_obj_t *r_energy = lv_obj_create(ui_test_result_panel);
    lv_obj_set_size(r_energy, 240, 60);
    lv_obj_set_style_bg_color(r_energy, UI_COLOR_SURFACE_EL, 0);
    lv_obj_set_style_bg_opa(r_energy, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(r_energy, 8, 0);
    lv_obj_set_style_border_width(r_energy, 0, 0);
    lv_obj_clear_flag(r_energy, LV_OBJ_FLAG_SCROLLABLE);

    ui_test_result_energy_label = lv_label_create(r_energy);
    lv_label_set_text(ui_test_result_energy_label, "-- J");
    lv_obj_set_style_text_font(ui_test_result_energy_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(ui_test_result_energy_label, UI_COLOR_SUCCESS, 0);
    lv_obj_center(ui_test_result_energy_label);

    /* ——— Action area (buttons change per state) ——— */
    action_area = lv_obj_create(content);
    lv_obj_set_size(action_area, 900, 80);
    lv_obj_align(action_area, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(action_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(action_area, 0, 0);
    lv_obj_set_style_pad_all(action_area, 0, 0);
    lv_obj_set_flex_flow(action_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_area, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(action_area, 20, 0);
    lv_obj_clear_flag(action_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Release button */
    ui_test_btn_release = lv_btn_create(action_area);
    lv_obj_set_size(ui_test_btn_release, 280, 64);
    lv_obj_set_style_bg_color(ui_test_btn_release, UI_COLOR_DANGER, 0);
    lv_obj_set_style_bg_opa(ui_test_btn_release, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_test_btn_release, 8, 0);
    lv_obj_add_event_cb(ui_test_btn_release, btn_release_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rel_lbl = lv_label_create(ui_test_btn_release);
    lv_label_set_text(rel_lbl, LV_SYMBOL_UPLOAD " RELEASE ARM");
    lv_obj_set_style_text_font(rel_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(rel_lbl, UI_COLOR_TEXT, 0);
    lv_obj_center(rel_lbl);
    lv_obj_add_flag(ui_test_btn_release, LV_OBJ_FLAG_HIDDEN);

    /* Abort button */
    ui_test_btn_abort = lv_btn_create(action_area);
    lv_obj_add_style(ui_test_btn_abort, &style_btn_danger, 0);
    lv_obj_set_size(ui_test_btn_abort, 200, 56);
    lv_obj_add_event_cb(ui_test_btn_abort, btn_abort_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *abort_lbl = lv_label_create(ui_test_btn_abort);
    lv_label_set_text(abort_lbl, LV_SYMBOL_CLOSE " ABORT TEST");
    lv_obj_set_style_text_font(abort_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(abort_lbl);

    /* Save button — removed (results are auto-saved on completion) */
    ui_test_btn_save = NULL;

    /* SAVE VOID button — visible only for not-cut results, lets operator record them */
    ui_test_btn_save_void = lv_btn_create(action_area);
    lv_obj_set_size(ui_test_btn_save_void, 200, 56);
    lv_obj_set_style_bg_color(ui_test_btn_save_void, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_opa(ui_test_btn_save_void, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_test_btn_save_void, 8, 0);
    lv_obj_add_event_cb(ui_test_btn_save_void, btn_save_void_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_void_lbl = lv_label_create(ui_test_btn_save_void);
    lv_label_set_text(save_void_lbl, LV_SYMBOL_SAVE " SAVE");
    lv_obj_set_style_text_font(save_void_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(save_void_lbl, lv_color_hex(0x000000), 0);
    lv_obj_center(save_void_lbl);
    lv_obj_add_flag(ui_test_btn_save_void, LV_OBJ_FLAG_HIDDEN);

    /* DONE / DISCARD button */
    ui_test_btn_discard = lv_btn_create(action_area);
    lv_obj_add_style(ui_test_btn_discard, &style_btn_danger, 0);
    lv_obj_set_size(ui_test_btn_discard, 200, 56);
    lv_obj_add_event_cb(ui_test_btn_discard, btn_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *disc_lbl = lv_label_create(ui_test_btn_discard);
    lv_label_set_text(disc_lbl, LV_SYMBOL_OK " DONE");
    lv_obj_set_style_text_font(disc_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(disc_lbl);
    lv_obj_add_flag(ui_test_btn_discard, LV_OBJ_FLAG_HIDDEN);

    /* RETRY button — shown only for void results in DBTT mode */
    ui_test_btn_retry = lv_btn_create(action_area);
    lv_obj_set_size(ui_test_btn_retry, 200, 56);
    lv_obj_set_style_bg_color(ui_test_btn_retry, UI_COLOR_WARNING, 0);
    lv_obj_set_style_bg_opa(ui_test_btn_retry, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_test_btn_retry, 8, 0);
    lv_obj_add_event_cb(ui_test_btn_retry, btn_retry_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *retry_lbl = lv_label_create(ui_test_btn_retry);
    lv_label_set_text(retry_lbl, LV_SYMBOL_REFRESH " RETRY");
    lv_obj_set_style_text_font(retry_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(retry_lbl, UI_COLOR_TEXT, 0);
    lv_obj_center(retry_lbl);
    lv_obj_add_flag(ui_test_btn_retry, LV_OBJ_FLAG_HIDDEN);

    /* Default view: HOMING (abort only) */
    ui_test_active_set_state(TEST_STATE_HOMING);
}

void ui_test_active_set_state(int state)
{
    /* Update progress dots */
    int filled = 0;
    lv_color_t arc_color = UI_COLOR_WARNING;
    const char *msg = "";

    switch (state) {
        case TEST_STATE_HOMING:
            filled = 1; arc_color = UI_COLOR_WARNING;
            msg = "HOMING - Moving arm to home position...";
            break;
        case TEST_STATE_HOMED_FWD:
            filled = 1; arc_color = UI_COLOR_WARNING;
            msg = "HOMING - Indexing forward...";
            break;
        case TEST_STATE_LATCH_OPENING:
            filled = 1; arc_color = UI_COLOR_WARNING;
            msg = "LATCHING - Opening release latch...";
            break;
        case TEST_STATE_RETURNING_HOME:
            filled = 1; arc_color = UI_COLOR_WARNING;
            msg = "LATCHING - Returning to home...";
            break;
        case TEST_STATE_LATCHED:
            filled = 1; arc_color = UI_COLOR_WARNING;
            msg = "LATCHED - Pendulum latched, arming...";
            break;
        case TEST_STATE_ARMING:
            filled = 1; arc_color = UI_COLOR_WARNING;
            msg = "ARMING - Lifting arm... Stand clear.";
            break;
        case TEST_STATE_ARMED:
            filled = 2; arc_color = UI_COLOR_SUCCESS;
            msg = "ARMED - Ready. Press RELEASE when ready.";
            break;
        case TEST_STATE_RELEASED:
        case TEST_STATE_MEASURING:
            filled = 3; arc_color = UI_COLOR_PRIMARY;
            msg = "MEASURING - Swing in progress... Hold steady.";
            break;
        case TEST_STATE_COMPLETE:
            filled = 4;
            if (test_manager_result_is_void()) {
                arc_color = UI_COLOR_WARNING;
                msg = "VOID - Specimen not cut. Discard or retry.";
            } else {
                arc_color = UI_COLOR_PRIMARY;
                msg = "COMPLETE - Test complete. Review results.";
            }
            if (ui_test_detail_label) lv_label_set_text(ui_test_detail_label, "");
            break;
        default:
            break;
    }

    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_color(ui_test_progress_dots[i],
            (i < filled) ? UI_COLOR_PRIMARY : UI_COLOR_TEXT_MUTED, 0);
    }
    lv_obj_set_style_arc_color(ui_test_arc, arc_color, LV_PART_INDICATOR);
    lv_label_set_text(ui_test_state_label, msg);

    /* Show/hide buttons based on state */
    bool show_release = (state == TEST_STATE_ARMED);
    bool show_abort   = (state == TEST_STATE_HOMING      ||
                         state == TEST_STATE_HOMED_FWD   ||
                         state == TEST_STATE_LATCH_OPENING ||
                         state == TEST_STATE_RETURNING_HOME ||
                         state == TEST_STATE_LATCHED     ||
                         state == TEST_STATE_ARMING      ||
                         state == TEST_STATE_ARMED);
    bool is_complete  = (state == TEST_STATE_COMPLETE);
    bool is_void      = is_complete && test_manager_result_is_void();
    bool show_done    = is_complete;                /* DONE (normal) or DISCARD (void) */
    bool show_result  = is_complete;                /* always show measured values */
    bool show_save_void = is_void;
    bool show_retry   = is_void;   /* available in both single and DBTT mode */

    /* Update DONE/DISCARD label */
    if (ui_test_btn_discard) {
        lv_obj_t *lbl = lv_obj_get_child(ui_test_btn_discard, 0);
        if (lbl) lv_label_set_text(lbl, is_void ? LV_SYMBOL_TRASH " DISCARD" : LV_SYMBOL_OK " DONE");
    }

    if (show_release) lv_obj_remove_flag(ui_test_btn_release, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_test_btn_release, LV_OBJ_FLAG_HIDDEN);

    if (show_abort) lv_obj_remove_flag(ui_test_btn_abort, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_test_btn_abort, LV_OBJ_FLAG_HIDDEN);

    if (show_done) lv_obj_remove_flag(ui_test_btn_discard, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_test_btn_discard, LV_OBJ_FLAG_HIDDEN);

    if (show_save_void) lv_obj_remove_flag(ui_test_btn_save_void, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_test_btn_save_void, LV_OBJ_FLAG_HIDDEN);

    if (show_retry) lv_obj_remove_flag(ui_test_btn_retry, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_test_btn_retry, LV_OBJ_FLAG_HIDDEN);

    if (show_result) lv_obj_remove_flag(ui_test_result_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui_test_result_panel, LV_OBJ_FLAG_HIDDEN);
}
