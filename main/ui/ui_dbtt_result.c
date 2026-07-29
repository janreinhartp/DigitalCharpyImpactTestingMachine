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

/* ——— History side-panel widgets ——— */
static lv_obj_t *s_hist_panel_table   = NULL;
static lv_obj_t *s_hist_count_label   = NULL;
static char      s_hist_files[DBTT_MAX_SESSIONS][32];
static int       s_hist_file_count    = 0;

/* ——— Axis tick label state (mirrored from chart ranges) ——— */
#define CHART_HDIV  5   /* must match lv_chart_set_div_line_count hdiv */
#define CHART_VDIV  5   /* must match lv_chart_set_div_line_count vdiv */
static int32_t s_chart_xmin = -20;
static int32_t s_chart_xmax =  100;
static int32_t s_chart_ymin =    0;
static int32_t s_chart_ymax =  100;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

/* Forward declaration — defined later in this file */
static void refresh_with_session(const dbtt_session_t *sess);

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

/* ——————————————— Axis tick label draw callback ——————————————— */

static void chart_axis_draw_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_POST_END) return;

    lv_obj_t   *chart = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    int32_t bw      = lv_obj_get_style_border_width(chart, 0);
    int32_t pad_left = lv_obj_get_style_pad_left(chart, 0);
    int32_t pad_top  = lv_obj_get_style_pad_top(chart, 0);
    int32_t cw      = lv_obj_get_content_width(chart);
    int32_t ch      = lv_obj_get_content_height(chart);

    /* Pixel origin of the chart's data area */
    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);
    int32_t cx1 = coords.x1 + pad_left + bw;
    int32_t cy1 = coords.y1 + pad_top  + bw;

    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font       = &lv_font_montserrat_14;
    dsc.color      = UI_COLOR_TEXT_SEC;
    dsc.opa        = LV_OPA_COVER;
    dsc.text_local = 1;

    char buf[16];

    /* ——— Y-axis labels (energy) — drawn in left padding, right-aligned ——— */
    dsc.align = LV_TEXT_ALIGN_RIGHT;
    for (int i = 0; i < CHART_HDIV; i++) {
        int32_t y_pixel = cy1 + ch * i / (CHART_HDIV - 1);
        int32_t y_val   = s_chart_ymax -
                          (int32_t)((int64_t)(s_chart_ymax - s_chart_ymin) * i / (CHART_HDIV - 1));
        snprintf(buf, sizeof(buf), "%d", (int)y_val);
        lv_area_t a = { .x1 = coords.x1, .y1 = y_pixel - 9,
                        .x2 = cx1 - 4,   .y2 = y_pixel + 9 };
        dsc.text = buf;
        lv_draw_label(layer, &dsc, &a);
    }

    /* ——— X-axis labels (temperature) — drawn in bottom padding, centered ——— */
    dsc.align = LV_TEXT_ALIGN_CENTER;
    for (int i = 0; i < CHART_VDIV; i++) {
        int32_t x_pixel = cx1 + cw * i / (CHART_VDIV - 1);
        int32_t x_val   = s_chart_xmin +
                          (int32_t)((int64_t)(s_chart_xmax - s_chart_xmin) * i / (CHART_VDIV - 1));
        snprintf(buf, sizeof(buf), "%d", (int)x_val);
        lv_area_t a = { .x1 = x_pixel - 22, .y1 = cy1 + ch + 2,
                        .x2 = x_pixel + 22,  .y2 = coords.y2 };
        dsc.text = buf;
        lv_draw_label(layer, &dsc, &a);
    }
}

/* ——————————————— History side-panel helpers ——————————————— */

static void hist_panel_table_evt_cb(lv_event_t *e)
{
    lv_obj_t *table = lv_event_get_target(e);
    uint32_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row == 0 || (int)row > s_hist_file_count) return;

    /* Newest-first: row 1 → last (newest) file */
    int idx = s_hist_file_count - (int)row;
    if (idx < 0 || idx >= s_hist_file_count) return;

    static dbtt_session_t loaded;
    if (dbtt_manager_load_session(s_hist_files[idx], &loaded) == ESP_OK) {
        memcpy(&s_hist_session, &loaded, sizeof(s_hist_session));
        s_history_mode = true;
        refresh_with_session(&s_hist_session);
        if (s_btn_save) lv_obj_add_flag(s_btn_save, LV_OBJ_FLAG_HIDDEN);
        if (s_btn_new)  lv_obj_add_flag(s_btn_new,  LV_OBJ_FLAG_HIDDEN);
    }
}

static void s_refresh_history_panel(void)
{
    if (!s_hist_panel_table || !s_hist_count_label) return;

    s_hist_file_count = dbtt_manager_list_sessions(s_hist_files, DBTT_MAX_SESSIONS);

    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d session%s",
             s_hist_file_count, s_hist_file_count == 1 ? "" : "s");
    lv_label_set_text(s_hist_count_label, count_buf);

    if (s_hist_file_count == 0) {
        lv_table_set_row_cnt(s_hist_panel_table, 2);
        lv_table_set_cell_value(s_hist_panel_table, 1, 0, "No saved sessions");
        lv_table_set_cell_value(s_hist_panel_table, 1, 1, "");
        return;
    }

    lv_table_set_row_cnt(s_hist_panel_table, (uint32_t)(s_hist_file_count + 1));

    for (int i = 0; i < s_hist_file_count; i++) {
        int fi = s_hist_file_count - 1 - i;   /* newest first */
        uint32_t row = (uint32_t)(i + 1);
        const char *fn = s_hist_files[fi];

        char date_str[24] = "--";
        if (strlen(fn) >= 20) {
            snprintf(date_str, sizeof(date_str),
                     "%.4s-%.2s-%.2s %.2s:%.2s",
                     fn + 5, fn + 9, fn + 11, fn + 14, fn + 16);
        }
        lv_table_set_cell_value(s_hist_panel_table, row, 0, date_str);

        static dbtt_session_t tmp;
        if (dbtt_manager_load_session(s_hist_files[fi], &tmp) == ESP_OK) {
            lv_table_set_cell_value(s_hist_panel_table, row, 1, tmp.material);
        } else {
            lv_table_set_cell_value(s_hist_panel_table, row, 1, "?");
        }
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
        s_chart_xmin = -20;  s_chart_xmax = 100;
        s_chart_ymin =   0;  s_chart_ymax = 100;
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
    s_chart_xmin = min_temp - 10;
    s_chart_xmax = max_temp  + 10;
    s_chart_ymin = 0;
    s_chart_ymax = max_energy + max_energy / 4 + 10;
    lv_chart_set_point_count(s_chart, (uint32_t)n);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y,
                       s_chart_ymin, s_chart_ymax);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_X,
                       s_chart_xmin, s_chart_xmax);

    for (int i = 0; i < n; i++) {
        lv_chart_set_value_by_id2(s_chart, s_ser, (uint32_t)i,
                                  (int32_t)roundf(sess->points[i].temperature_c),
                                  (int32_t)roundf(sess->points[i].energy_joules));
    }
    lv_chart_refresh(s_chart);

    /* ——— Data table ——— */
    lv_table_set_col_cnt(s_table, 5);
    lv_table_set_col_width(s_table, 0,  50);  /* # */
    lv_table_set_col_width(s_table, 1, 130);  /* Temp */
    lv_table_set_col_width(s_table, 2, 130);  /* Energy */
    lv_table_set_col_width(s_table, 3, 120);  /* Angle */
    lv_table_set_col_width(s_table, 4, 200);  /* Specimen */

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
    s_refresh_history_panel();
}

void ui_dbtt_result_load(const dbtt_session_t *sess)
{
    if (!sess) return;
    memcpy(&s_hist_session, sess, sizeof(s_hist_session));
    s_history_mode = true;
    refresh_with_session(&s_hist_session);
    if (s_btn_save) lv_obj_add_flag(s_btn_save, LV_OBJ_FLAG_HIDDEN);
    if (s_btn_new)  lv_obj_add_flag(s_btn_new,  LV_OBJ_FLAG_HIDDEN);
    s_refresh_history_panel();
}

/* ——————————————— Screen constructor ——————————————— */

void ui_dbtt_result_create(void)
{
    scr_dbtt_result = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dbtt_result, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_dbtt_result, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_dbtt_result, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_dbtt_result);

    /* ——— Outer content area (ROW: left chart/table | right history panel) ——— */
    /* 1024 - 2×12 = 1000; left=660, gap=10, right=330 */
    lv_obj_t *content = lv_obj_create(scr_dbtt_result);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_column(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* ════════════════════════════════════════════════════════
     *  LEFT PANEL — chart + data table
     * ════════════════════════════════════════════════════════ */
    lv_obj_t *left = lv_obj_create(content);
    lv_obj_set_size(left, 660, lv_pct(100));
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 6, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    /* ——— Header row ——— */
    lv_obj_t *header = lv_obj_create(left);
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
    lv_obj_set_size(title_col, 340, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_col, 0, 0);
    lv_obj_set_style_pad_all(title_col, 0, 0);
    lv_obj_set_flex_flow(title_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(title_col, 4, 0);
    lv_obj_clear_flag(title_col, LV_OBJ_FLAG_SCROLLABLE);

    s_title_label = lv_label_create(title_col);
    lv_label_set_text(s_title_label, "DBTT Results");
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_title_label, UI_COLOR_TEXT, 0);

    s_status_label = lv_label_create(title_col);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT_SEC, 0);

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(header);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 8, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_save = lv_btn_create(btn_row);
    lv_obj_add_style(s_btn_save, &style_btn_primary, 0);
    lv_obj_set_size(s_btn_save, 130, 38);
    lv_obj_add_event_cb(s_btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_lbl = lv_label_create(s_btn_save);
    lv_label_set_text(save_lbl, LV_SYMBOL_SD_CARD " Save CSV");
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(save_lbl);

    s_btn_new = lv_btn_create(btn_row);
    lv_obj_add_style(s_btn_new, &style_btn_secondary, 0);
    lv_obj_set_size(s_btn_new, 100, 38);
    lv_obj_add_event_cb(s_btn_new, btn_new_session_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *new_lbl = lv_label_create(s_btn_new);
    lv_label_set_text(new_lbl, LV_SYMBOL_REFRESH " New");
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(new_lbl);

    lv_obj_t *btn_back = lv_btn_create(btn_row);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, 90, 38);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(back_lbl);

    /* ——— Y-axis label ——— */
    lv_obj_t *y_lbl = lv_label_create(left);
    lv_label_set_text(y_lbl, "Absorbed Energy (J)");
    lv_obj_set_style_text_font(y_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(y_lbl, UI_COLOR_TEXT_SEC, 0);

    /* ——— Scatter chart ——— */
    s_chart = lv_chart_create(left);
    lv_obj_set_size(s_chart, lv_pct(100), 200);
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

    /* Extra padding for axis tick labels */
    lv_obj_set_style_pad_left(s_chart, 38, 0);
    lv_obj_set_style_pad_bottom(s_chart, 18, 0);
    lv_obj_add_event_cb(s_chart, chart_axis_draw_cb, LV_EVENT_DRAW_POST_END, NULL);

    /* ——— X-axis label ——— */
    lv_obj_t *x_lbl = lv_label_create(left);
    lv_label_set_text(x_lbl, "Temperature (\xc2\xb0""C)");
    lv_obj_set_style_text_font(x_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(x_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_align(x_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(x_lbl, lv_pct(100));

    /* ——— Results table (in scrollable wrapper) ——— */
    lv_obj_t *tbl_wrap = lv_obj_create(left);
    lv_obj_set_size(tbl_wrap, lv_pct(100), 200);
    lv_obj_set_style_bg_color(tbl_wrap, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(tbl_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tbl_wrap, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(tbl_wrap, 1, 0);
    lv_obj_set_style_radius(tbl_wrap, 8, 0);
    lv_obj_set_style_pad_all(tbl_wrap, 0, 0);
    /* tbl_wrap is scrollable by default in LVGL 9 */

    s_table = lv_table_create(tbl_wrap);
    lv_obj_set_style_text_color(s_table, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_table, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_table, UI_COLOR_TEXT_SEC, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_table, UI_COLOR_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(s_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(s_table, 5, LV_PART_ITEMS);

    /* ════════════════════════════════════════════════════════
     *  RIGHT PANEL — session history list
     * ════════════════════════════════════════════════════════ */
    lv_obj_t *right = lv_obj_create(content);
    lv_obj_set_size(right, 330, lv_pct(100));
    lv_obj_set_style_bg_color(right, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(right, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_radius(right, 8, 0);
    lv_obj_set_style_pad_all(right, 10, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 6, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    /* History panel header */
    lv_obj_t *hist_hdr = lv_obj_create(right);
    lv_obj_set_size(hist_hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hist_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hist_hdr, 0, 0);
    lv_obj_set_style_pad_all(hist_hdr, 0, 0);
    lv_obj_set_flex_flow(hist_hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hist_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hist_hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hist_title = lv_label_create(hist_hdr);
    lv_label_set_text(hist_title, "Session History");
    lv_obj_set_style_text_font(hist_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hist_title, UI_COLOR_TEXT, 0);

    s_hist_count_label = lv_label_create(hist_hdr);
    lv_label_set_text(s_hist_count_label, "0 sessions");
    lv_obj_set_style_text_font(s_hist_count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hist_count_label, UI_COLOR_TEXT_SEC, 0);

    /* Hint label */
    lv_obj_t *hist_hint = lv_label_create(right);
    lv_label_set_text(hist_hint, "Tap a row to load");
    lv_obj_set_style_text_font(hist_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hist_hint, UI_COLOR_TEXT_SEC, 0);

    /* Divider */
    lv_obj_t *divider = lv_obj_create(right);
    lv_obj_set_size(divider, lv_pct(100), 1);
    lv_obj_set_style_bg_color(divider, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);

    /* Sessions table wrapper (scrollable) */
    lv_obj_t *hist_wrap = lv_obj_create(right);
    lv_obj_set_size(hist_wrap, lv_pct(100), 450);
    lv_obj_set_style_bg_opa(hist_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hist_wrap, 0, 0);
    lv_obj_set_style_pad_all(hist_wrap, 0, 0);

    s_hist_panel_table = lv_table_create(hist_wrap);
    lv_obj_set_style_bg_color(s_hist_panel_table, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_hist_panel_table, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(s_hist_panel_table, UI_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_hist_panel_table, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_hist_panel_table, UI_COLOR_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_hist_panel_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(s_hist_panel_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(s_hist_panel_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_hist_panel_table, UI_COLOR_SURFACE_EL,
                              LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_table_set_col_cnt(s_hist_panel_table, 2);
    lv_table_set_col_width(s_hist_panel_table, 0, 170);  /* Date/Time */
    lv_table_set_col_width(s_hist_panel_table, 1, 130);  /* Material  */

    lv_table_set_cell_value(s_hist_panel_table, 0, 0, "Date / Time");
    lv_table_set_cell_value(s_hist_panel_table, 0, 1, "Material");

    lv_obj_add_event_cb(s_hist_panel_table, hist_panel_table_evt_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
}
