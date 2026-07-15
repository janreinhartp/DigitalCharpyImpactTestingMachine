/*
 * ui_dbtt_history.c — DBTT Session History screen.
 *
 * Lists all saved DBTT sessions (DBTT_*.dat) from the SD card.
 * Tap any row to load that session and view its scatter chart.
 */

#include "ui.h"
#include "dbtt_manager.h"
#include <string.h>

extern lv_obj_t *scr_dbtt_history;
extern void ui_create_screen_status_bar(lv_obj_t *screen);
extern void ui_dbtt_result_load(const dbtt_session_t *sess);

#define MAX_HIST_SESSIONS  DBTT_MAX_SESSIONS

static char      s_files[MAX_HIST_SESSIONS][32];
static int       s_file_count = 0;

static lv_obj_t *s_table       = NULL;
static lv_obj_t *s_count_label = NULL;

/* ——————————————— Callbacks ——————————————— */

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    ui_show_dbtt();
}

static void table_evt_cb(lv_event_t *e)
{
    lv_obj_t *table = lv_event_get_target(e);
    uint32_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row == 0 || (int)row > s_file_count) return;

    /* Display newest-first: row 1 → last file in sorted list */
    int idx = s_file_count - (int)row;
    if (idx < 0) return;

    static dbtt_session_t hist_sess;
    if (dbtt_manager_load_session(s_files[idx], &hist_sess) == ESP_OK) {
        ui_dbtt_result_load(&hist_sess);
        ui_show_dbtt_result_history();
    }
}

/* ——————————————— Public refresh ——————————————— */

void ui_dbtt_history_refresh(void)
{
    s_file_count = dbtt_manager_list_sessions(s_files, MAX_HIST_SESSIONS);

    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d session%s",
             s_file_count, s_file_count == 1 ? "" : "s");
    lv_label_set_text(s_count_label, count_buf);

    if (s_file_count == 0) {
        lv_table_set_row_cnt(s_table, 2);
        lv_table_set_cell_value(s_table, 1, 0, "No sessions saved yet — complete a DBTT session first");
        lv_table_set_cell_value(s_table, 1, 1, "");
        lv_table_set_cell_value(s_table, 1, 2, "");
        lv_table_set_cell_value(s_table, 1, 3, "");
        return;
    }

    lv_table_set_row_cnt(s_table, (uint32_t)(s_file_count + 1));

    for (int i = 0; i < s_file_count; i++) {
        /* Newest first: i=0 shows the last (newest) file */
        int fi = s_file_count - 1 - i;
        uint32_t row = (uint32_t)(i + 1);

        /* Parse date/time from filename "DBTT_YYYYMMDD_HHMMSS.dat" (length 24) */
        const char *fn = s_files[fi];
        char date_str[24] = "--";
        if (strlen(fn) >= 20) {
            /* fn: D B T T _ Y Y Y Y M M D D _ H H M M S S . d a t
               idx: 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 ...    */
            snprintf(date_str, sizeof(date_str),
                     "%.4s-%.2s-%.2s  %.2s:%.2s",
                     fn + 5, fn + 9, fn + 11, fn + 14, fn + 16);
        }
        lv_table_set_cell_value(s_table, row, 0, date_str);

        static dbtt_session_t tmp;
        if (dbtt_manager_load_session(s_files[fi], &tmp) == ESP_OK) {
            lv_table_set_cell_value(s_table, row, 1, tmp.material);
            lv_table_set_cell_value(s_table, row, 2,
                                    tmp.operator_name[0] ? tmp.operator_name : "--");
            char n_buf[8];
            snprintf(n_buf, sizeof(n_buf), "%d", tmp.n_done);
            lv_table_set_cell_value(s_table, row, 3, n_buf);
        } else {
            lv_table_set_cell_value(s_table, row, 1, "?");
            lv_table_set_cell_value(s_table, row, 2, "?");
            lv_table_set_cell_value(s_table, row, 3, "?");
        }
    }
}

/* ——————————————— Screen constructor ——————————————— */

void ui_dbtt_history_create(void)
{
    scr_dbtt_history = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dbtt_history, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_dbtt_history, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_dbtt_history, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_dbtt_history);

    /* Content area */
    lv_obj_t *content = lv_obj_create(scr_dbtt_history);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 8, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* Header */
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
    lv_label_set_text(title, "DBTT Session History");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    s_count_label = lv_label_create(header);
    lv_label_set_text(s_count_label, "0 sessions");
    lv_obj_set_style_text_color(s_count_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_count_label, &lv_font_montserrat_16, 0);

    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, 110, 40);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(back_lbl);

    /* Hint */
    lv_obj_t *hint = lv_label_create(content);
    lv_label_set_text(hint, "Tap a row to view its scatter chart");
    lv_obj_set_style_text_color(hint, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);

    /* Table */
    s_table = lv_table_create(content);
    lv_obj_set_size(s_table, lv_pct(100), 450);
    lv_obj_set_style_bg_color(s_table, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_table, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(s_table, 1, 0);
    lv_obj_set_style_radius(s_table, 8, 0);

    lv_table_set_col_cnt(s_table, 4);
    lv_table_set_col_width(s_table, 0, 260);  /* Date / Time */
    lv_table_set_col_width(s_table, 1, 240);  /* Material    */
    lv_table_set_col_width(s_table, 2, 360);  /* Operator    */
    lv_table_set_col_width(s_table, 3, 100);  /* Tests       */

    lv_obj_set_style_text_color(s_table, UI_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_table, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_table, UI_COLOR_SURFACE_EL,
                              LV_PART_ITEMS | LV_STATE_PRESSED);

    /* Column headers */
    lv_table_set_cell_value(s_table, 0, 0, "Date / Time");
    lv_table_set_cell_value(s_table, 0, 1, "Material");
    lv_table_set_cell_value(s_table, 0, 2, "Operator");
    lv_table_set_cell_value(s_table, 0, 3, "Tests");

    lv_obj_add_event_cb(s_table, table_evt_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
