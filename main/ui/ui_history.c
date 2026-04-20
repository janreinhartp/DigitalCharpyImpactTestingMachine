#include "ui.h"
#include "data_logger.h"

/* History widgets */
lv_obj_t *ui_hist_table   = NULL;
lv_obj_t *ui_hist_count   = NULL;
lv_obj_t *ui_hist_page    = NULL;

#define ROWS_PER_PAGE 10
static int current_page   = 0;
static int total_records  = 0;

extern void ui_create_screen_status_bar(lv_obj_t *screen);

static void btn_back_cb(lv_event_t *e) { (void)e; ui_show_dashboard(); }

static void update_table(void);

static void btn_prev_cb(lv_event_t *e)
{
    (void)e;
    if (current_page > 0) {
        current_page--;
        update_table();
    }
}

static void btn_next_cb(lv_event_t *e)
{
    (void)e;
    int max_page = (total_records > 0) ? ((total_records - 1) / ROWS_PER_PAGE) : 0;
    if (current_page < max_page) {
        current_page++;
        update_table();
    }
}

static void update_table(void)
{
    /* Set column count & header row */
    lv_table_set_col_cnt(ui_hist_table, 6);
    lv_table_set_col_width(ui_hist_table, 0, 180);   /* Date */
    lv_table_set_col_width(ui_hist_table, 1, 180);   /* Specimen */
    lv_table_set_col_width(ui_hist_table, 2, 155);   /* Material */
    lv_table_set_col_width(ui_hist_table, 3, 120);   /* Angle */
    lv_table_set_col_width(ui_hist_table, 4, 120);   /* Energy */
    lv_table_set_col_width(ui_hist_table, 5, 175);   /* Status */

    /* Header */
    lv_table_set_cell_value(ui_hist_table, 0, 0, "Date/Time");
    lv_table_set_cell_value(ui_hist_table, 0, 1, "Specimen");
    lv_table_set_cell_value(ui_hist_table, 0, 2, "Material");
    lv_table_set_cell_value(ui_hist_table, 0, 3, "Angle(°)");
    lv_table_set_cell_value(ui_hist_table, 0, 4, "Energy(J)");
    lv_table_set_cell_value(ui_hist_table, 0, 5, "Status");

    const test_result_t *history = NULL;
    total_records = 0;
    data_logger_get_history(&history, &total_records);

    int start = current_page * ROWS_PER_PAGE;
    int end = start + ROWS_PER_PAGE;
    if (end > total_records) end = total_records;
    int row_count = (history != NULL) ? (end - start) : 0;
    lv_table_set_row_cnt(ui_hist_table, row_count + 1);

    for (int i = 0; i < row_count; i++) {
        int idx = total_records - 1 - (start + i); /* newest first */
        if (idx < 0) break;
        const test_result_t *r = &history[idx];

        lv_table_set_cell_value(ui_hist_table, i + 1, 0, r->timestamp);
        lv_table_set_cell_value(ui_hist_table, i + 1, 1, r->specimen.specimen_id);
        lv_table_set_cell_value(ui_hist_table, i + 1, 2, r->specimen.material);

        char angle_buf[16]; char energy_buf[16];
        snprintf(angle_buf, sizeof(angle_buf), "%.1f", r->final_angle_deg);
        snprintf(energy_buf, sizeof(energy_buf), "%.2f", r->energy_joules);
        lv_table_set_cell_value(ui_hist_table, i + 1, 3, angle_buf);
        lv_table_set_cell_value(ui_hist_table, i + 1, 4, energy_buf);
        lv_table_set_cell_value(ui_hist_table, i + 1, 5, (r->energy_joules > 0) ? "OK" : "--");
    }

    /* Status labels */
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d records", total_records);
    lv_label_set_text(ui_hist_count, count_buf);

    int max_page = (total_records > 0) ? ((total_records - 1) / ROWS_PER_PAGE) : 0;
    char page_buf[32];
    snprintf(page_buf, sizeof(page_buf), "Page %d / %d", current_page + 1, max_page + 1);
    lv_label_set_text(ui_hist_page, page_buf);
}

void ui_history_create(void)
{
    scr_history = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_history, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr_history, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_history, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_screen_status_bar(scr_history);

    /* Content */
    lv_obj_t *content = lv_obj_create(scr_history);
    lv_obj_set_size(content, 1024, 560);
    lv_obj_set_pos(content, 0, UI_STATUS_BAR_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

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
    lv_label_set_text(title, LV_SYMBOL_LIST " Test History");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);

    ui_hist_count = lv_label_create(header);
    lv_label_set_text(ui_hist_count, "0 records");
    lv_obj_set_style_text_color(ui_hist_count, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_hist_count, &lv_font_montserrat_16, 0);

    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_add_style(btn_back, &style_btn_secondary, 0);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_lbl);

    /* Table */
    ui_hist_table = lv_table_create(content);
    lv_obj_set_size(ui_hist_table, lv_pct(100), 420);
    lv_obj_set_style_bg_color(ui_hist_table, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(ui_hist_table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ui_hist_table, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ui_hist_table, 1, 0);
    lv_obj_set_style_radius(ui_hist_table, 8, 0);
    lv_obj_set_style_text_color(ui_hist_table, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_hist_table, &lv_font_montserrat_16, 0);

    /* Header row styling */
    lv_obj_set_style_text_color(ui_hist_table, UI_COLOR_TEXT_SEC, LV_PART_ITEMS);
    lv_obj_set_style_border_color(ui_hist_table, UI_COLOR_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(ui_hist_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(ui_hist_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(ui_hist_table, 6, LV_PART_ITEMS);

    /* Footer: pagination */
    lv_obj_t *footer = lv_obj_create(content);
    lv_obj_set_size(footer, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, 20, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_prev = lv_btn_create(footer);
    lv_obj_add_style(btn_prev, &style_btn_secondary, 0);
    lv_obj_set_size(btn_prev, 100, 36);
    lv_obj_add_event_cb(btn_prev, btn_prev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *prev_lbl = lv_label_create(btn_prev);
    lv_label_set_text(prev_lbl, LV_SYMBOL_LEFT " Prev");
    lv_obj_center(prev_lbl);

    ui_hist_page = lv_label_create(footer);
    lv_label_set_text(ui_hist_page, "Page 1 / 1");
    lv_obj_set_style_text_color(ui_hist_page, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_hist_page, &lv_font_montserrat_16, 0);

    lv_obj_t *btn_next = lv_btn_create(footer);
    lv_obj_add_style(btn_next, &style_btn_secondary, 0);
    lv_obj_set_size(btn_next, 100, 36);
    lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *next_lbl = lv_label_create(btn_next);
    lv_label_set_text(next_lbl, "Next " LV_SYMBOL_RIGHT);
    lv_obj_center(next_lbl);

    /* Populate initial data */
    update_table();
}

void ui_history_refresh(void)
{
    current_page = 0;
    update_table();
}
