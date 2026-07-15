/*
 * ui_dbtt_result.c — DBTT Results Screen.
 *
 * Displays the completed (or partial) DBTT session as:
 *   • Scatter chart: X = Temperature (°C), Y = Absorbed Energy (J)
 *   • Data table:    Test# | Temp (°C) | Energy (J) | Angle (°) | Specimen ID
 *
 * Buttons: Save CSV → SD card  |  New Session → setup  |  Back → Dashboard
 */

#include "ui.h"
#include "dbtt_manager.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ——————————————— Screen + widget handles (file-scope) ——————————————— */

extern lv_obj_t *scr_dbtt_result;

static lv_obj_t          *s_title_label  = NULL;
static lv_obj_t          *s_status_label = NULL;
static lv_obj_t          *s_chart        = NULL;
static lv_chart_series_t *s_ser          = NULL;
static lv_obj_t          *s_table        = NULL;
static lv_obj_t          *s_btn_save     = NULL;
static lv_obj_t          *s_btn_new      = NULL;

/* History-mode: viewing a loaded past session instead of the live one */
static bool            s_history_mode = false;
static dbtt_session_t  s_hist_session;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

/* ——————————————— Callbacks ——————————————— */

static void btn_save_cb(lv_event_t *e)
{
    (void)e;
    esp_err_t ret = dbtt_manager_save_csv();
    if (ret == ESP_OK) {
        lv_label_set_text(s_status_label, "Saved >> /sdcard/DBTT_results.csv");
        lv_obj_set_style_text_color(s_status_label, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(s_status_label, "Save failed - SD card not available");
        lv_obj_set_style_text_color(s_status_label, UI_COLOR_DANGER, 0);
    }
}

static void btn_new_session_cb(lv_event_t *e)
{
    (void)e;
    dbtt_manager_reset();
    ui_show_dbtt();          /* back to setup screen */
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_history_mode) {
        ui_show_dbtt_history();
    } else {
        ui_show_dashboard();
    }
}

/* ——————————————— Internal refresh helper ——————————————— */

static void refresh_with_session(const dbtt_session_t *sess)
{
    int n = sess->n_done;

    /* ——— Title ——— */
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "DBTT Results - %s  (%d tests)",
                 sess->material, n);
        lv_label_set_text(s_title_label, buf);
    }

    /* ——— Clear status label ——— */
    lv_label_set_text(s_status_label, s_history_mode ? "(history)" : "");

    if (n == 0) {
        lv_chart_set_point_count(s_chart, 1);
        lv_chart_set_all_value(s_chart, s_ser, LV_CHART_POINT_NONE);
        lv_chart_refresh(s_chart);
        lv_table_set_row_cnt(s_table, 1);
        return;
    }

    /* ——— Find value ranges ——— */
    int32_t min_temp = (int32_t)roundf(sess->points[0].temperature_c);
    int32_t max_temp = min_temp;
    int32_t max_energy = 1;

    for (int i = 0; i < n; i++) {
        int32_t t = (int32_t)roundf(sess->points[i].temperature_c);
        int32_t e = (int32_t)roundf(sess->points[i].energy_joules);
        if (t < min_temp) min_temp = t;
        if (t > max_temp) max_temp = t;
        if (e > max_energy) max_energy = e;
    }

    /* Ensure X range has some padding */
    if (min_temp == max_temp) { min_temp -= 10; max_temp += 10; }

    /* ——— Configure scatter chart ——— */
    lv_chart_set_point_count(s_chart, (uint32_t)n);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y,
                       0, max_energy + max_energy / 4 + 10);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_X,
                       min_temp - 10, max_temp + 10);

    for (int i = 0; i < n; i++) {
        lv_chart_set_value_by_id2(s_chart, s_ser, (uint32_t)i,
                                  (int32_t)roundf(sess->points[i].temperature_c),
                                  (int32_t)roundf(sess->points[i].energy_joules));
    }
    lv_chart_refresh(s_chart);

    /* ——— Data table ——— */
    lv_table_set_col_cnt(s_table, 5);
    lv_table_set_col_width(s_table, 0,  60);  /* # */
    lv_table_set_col_width(s_table, 1, 170);  /* Temp */
    lv_table_set_col_width(s_table, 2, 170);  /* Energy */
    lv_table_set_col_width(s_table, 3, 160);  /* Angle */
    lv_table_set_col_width(s_table, 4, 220);  /* Specimen */

    lv_table_set_cell_value(s_table, 0, 0, "#");
    lv_table_set_cell_value(s_table, 0, 1, "Temp (\xc2\xb0""C)");
    lv_table_set_cell_value(s_table, 0, 2, "Energy (J)");
    lv_table_set_cell_value(s_table, 0, 3, "Angle (\xc2\xb0)");
    lv_table_set_cell_value(s_table, 0, 4, "Specimen");

    lv_table_set_row_cnt(s_table, (uint32_t)(n + 1));

    for (int i = 0; i < n; i++) {
        char buf[32];
        const dbtt_point_t *p = &sess->points[i];

        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_table_set_cell_value(s_table, (uint32_t)(i + 1), 0, buf);

        snprintf(buf, sizeof(buf), "%.1f", (double)p->temperature_c);
        lv_table_set_cell_value(s_table, (uint32_t)(i + 1), 1, buf);

        snprintf(buf, sizeof(buf), "%.2f", (double)p->energy_joules);
        lv_table_set_cell_value(s_table, (uint32_t)(i + 1), 2, buf);

        snprintf(buf, sizeof(buf), "%.1f", (double)p->final_angle_deg);
        lv_table_set_cell_value(s_table, (uint32_t)(i + 1), 3, buf);

        lv_table_set_cell_value(s_table, (uint32_t)(i + 1), 4, p->specimen_id);
    }
}

/* ——————————————— Public refresh ——————————————— */

void ui_dbtt_result_refresh(void)
{
    s_history_mode = false;
    refresh_with_session(dbtt_manager_get_session());
    if (s_btn_save) lv_obj_clear_flag(s_btn_save, LV_OBJ_FLAG_HIDDEN);
    if (s_btn_new)  lv_obj_clear_flag(s_btn_new,  LV_OBJ_FLAG_HIDDEN);
}

void ui_dbtt_result_load(const dbtt_session_t *sess)
{
    if (!sess) return;
    memcpy(&s_hist_session, sess, sizeof(s_hist_session));
    s_history_mode = true;
    refresh_with_session(&s_hist_session);
    if (s_btn_save) lv_obj_add_flag(s_btn_save, LV_OBJ_FLAG_HIDDEN);
    if (s_btn_new)  lv_obj_add_flag(s_btn_new,  LV_OBJ_FLAG_HIDDEN);
}

/* ——————————————— Screen constructor ——————————————— */

void ui_dbtt_result_create(void)
{
    scr_dbtt_result = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dbtt_result, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_dbtt_result, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_dbtt_result, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_dbtt_result);

    /* ——— Content area ——— */
    lv_obj_t *content = lv_obj_create(scr_dbtt_result);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ——— Header row ——— */
    lv_obj_t *header = lv_obj_create(content);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Title + status column */
    lv_obj_t *title_col = lv_obj_create(header);
    lv_obj_set_size(title_col, 500, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_col, 0, 0);
    lv_obj_set_style_pad_all(title_col, 0, 0);
    lv_obj_set_flex_flow(title_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(title_col, 4, 0);
    lv_obj_clear_flag(title_col, LV_OBJ_FLAG_SCROLLABLE);

    s_title_label = lv_label_create(title_col);
    lv_label_set_text(s_title_label, "DBTT Results");
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_title_label, UI_COLOR_TEXT, 0);

    s_status_label = lv_label_create(title_col);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT_SEC, 0);

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(header);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_save = lv_btn_create(btn_row);
    lv_obj_add_style(s_btn_save, &style_btn_primary, 0);
    lv_obj_set_size(s_btn_save, 150, 40);
    lv_obj_add_event_cb(s_btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_lbl = lv_label_create(s_btn_save);
    lv_label_set_text(save_lbl, LV_SYMBOL_SD_CARD " Save CSV");
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(save_lbl);

    s_btn_new = lv_btn_create(btn_row);
    lv_obj_add_style(s_btn_new, &style_btn_secondary, 0);
    lv_obj_set_size(s_btn_new, 150, 40);
    lv_obj_add_event_cb(s_btn_new, btn_new_session_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *new_lbl = lv_label_create(s_btn_new);
    lv_label_set_text(new_lbl, LV_SYMBOL_REFRESH " New");
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(new_lbl);

    lv_obj_t *btn_back = lv_btn_create(btn_row);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(back_lbl);

    /* ——— Y-axis label ——— */
    lv_obj_t *y_lbl = lv_label_create(content);
    lv_label_set_text(y_lbl, "Absorbed Energy (J)");
    lv_obj_set_style_text_font(y_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(y_lbl, UI_COLOR_TEXT_SEC, 0);

    /* ——— Scatter chart ——— */
    s_chart = lv_chart_create(content);
    lv_obj_set_size(s_chart, lv_pct(100), 230);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_point_count(s_chart, 1);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_X, -20, 100);
    lv_chart_set_div_line_count(s_chart, 5, 5);

    lv_obj_set_style_bg_color(s_chart, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_chart, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(s_chart, 1, 0);
    lv_obj_set_style_radius(s_chart, 8, 0);
    lv_obj_set_style_pad_all(s_chart, 10, 0);
    lv_obj_set_style_line_color(s_chart, UI_COLOR_BORDER, LV_PART_MAIN);

    /* Dot appearance */
    lv_obj_set_style_width(s_chart, 14, LV_PART_INDICATOR);
    lv_obj_set_style_height(s_chart, 14, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_chart, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    s_ser = lv_chart_add_series(s_chart, UI_COLOR_PRIMARY, LV_CHART_AXIS_PRIMARY_Y);

    /* ——— X-axis label ——— */
    lv_obj_t *x_lbl = lv_label_create(content);
    lv_label_set_text(x_lbl, "Temperature (\xc2\xb0""C)");
    lv_obj_set_style_text_font(x_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(x_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_align(x_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(x_lbl, lv_pct(100));

    /* ——— Results table (in scrollable wrapper) ——— */
    lv_obj_t *tbl_wrap = lv_obj_create(content);
    lv_obj_set_size(tbl_wrap, lv_pct(100), 170);
    lv_obj_set_style_bg_color(tbl_wrap, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(tbl_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tbl_wrap, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(tbl_wrap, 1, 0);
    lv_obj_set_style_radius(tbl_wrap, 8, 0);
    lv_obj_set_style_pad_all(tbl_wrap, 0, 0);
    /* tbl_wrap is scrollable by default in LVGL 9 */

    s_table = lv_table_create(tbl_wrap);
    lv_obj_set_style_text_color(s_table, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_table, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_table, UI_COLOR_TEXT_SEC, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_table, UI_COLOR_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(s_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(s_table, 6, LV_PART_ITEMS);
}
