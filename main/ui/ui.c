#include "ui.h"

/*——————————————— Shared Styles ———————————————*/
lv_style_t style_card;
lv_style_t style_btn_primary;
lv_style_t style_btn_secondary;
lv_style_t style_btn_danger;
lv_style_t style_btn_warning;
lv_style_t style_input;

/*——————————————— Screen Objects ———————————————*/
lv_obj_t *scr_dashboard    = NULL;
lv_obj_t *scr_specimen     = NULL;
lv_obj_t *scr_test_active  = NULL;
lv_obj_t *scr_history      = NULL;
lv_obj_t *scr_settings     = NULL;
lv_obj_t *scr_dbtt_setup   = NULL;
lv_obj_t *scr_dbtt_run     = NULL;
lv_obj_t *scr_dbtt_result  = NULL;
lv_obj_t *scr_dbtt_history = NULL;

/*——————————————— Status Bar ———————————————*/
lv_obj_t *ui_status_time_label  = NULL;
lv_obj_t *ui_status_date_label  = NULL;
lv_obj_t *ui_status_state_label = NULL;
lv_obj_t *ui_status_dot         = NULL;
lv_obj_t *ui_status_sd_label    = NULL;

static void init_styles(void)
{
    /* Card style */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, UI_COLOR_SURFACE);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_border_color(&style_card, UI_COLOR_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_pad_all(&style_card, 16);

    /* Primary button */
    lv_style_init(&style_btn_primary);
    lv_style_set_bg_color(&style_btn_primary, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_btn_primary, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_primary, UI_COLOR_BG);
    lv_style_set_radius(&style_btn_primary, 8);
    lv_style_set_pad_ver(&style_btn_primary, 12);
    lv_style_set_pad_hor(&style_btn_primary, 24);

    /* Secondary button */
    lv_style_init(&style_btn_secondary);
    lv_style_set_bg_color(&style_btn_secondary, UI_COLOR_SURFACE);
    lv_style_set_bg_opa(&style_btn_secondary, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn_secondary, UI_COLOR_PRIMARY);
    lv_style_set_border_width(&style_btn_secondary, 2);
    lv_style_set_text_color(&style_btn_secondary, UI_COLOR_PRIMARY);
    lv_style_set_radius(&style_btn_secondary, 8);
    lv_style_set_pad_ver(&style_btn_secondary, 12);
    lv_style_set_pad_hor(&style_btn_secondary, 24);

    /* Danger button */
    lv_style_init(&style_btn_danger);
    lv_style_set_bg_color(&style_btn_danger, UI_COLOR_SURFACE);
    lv_style_set_bg_opa(&style_btn_danger, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn_danger, UI_COLOR_DANGER);
    lv_style_set_border_width(&style_btn_danger, 2);
    lv_style_set_text_color(&style_btn_danger, UI_COLOR_DANGER);
    lv_style_set_radius(&style_btn_danger, 8);
    lv_style_set_pad_ver(&style_btn_danger, 12);
    lv_style_set_pad_hor(&style_btn_danger, 24);

    /* Warning button */
    lv_style_init(&style_btn_warning);
    lv_style_set_bg_color(&style_btn_warning, UI_COLOR_SURFACE);
    lv_style_set_bg_opa(&style_btn_warning, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn_warning, UI_COLOR_WARNING);
    lv_style_set_border_width(&style_btn_warning, 2);
    lv_style_set_text_color(&style_btn_warning, UI_COLOR_WARNING);
    lv_style_set_radius(&style_btn_warning, 8);
    lv_style_set_pad_ver(&style_btn_warning, 12);
    lv_style_set_pad_hor(&style_btn_warning, 24);

    /* Input field */
    lv_style_init(&style_input);
    lv_style_set_bg_color(&style_input, UI_COLOR_SURFACE);
    lv_style_set_bg_opa(&style_input, LV_OPA_COVER);
    lv_style_set_border_color(&style_input, UI_COLOR_BORDER);
    lv_style_set_border_width(&style_input, 1);
    lv_style_set_text_color(&style_input, UI_COLOR_TEXT);
    lv_style_set_radius(&style_input, 6);
    lv_style_set_pad_all(&style_input, 8);
}

/**
 * @brief Create status bar on a screen
 */
static void create_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 1024, UI_STATUS_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Left: status dot + title */
    lv_obj_t *left = lv_obj_create(bar);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 8, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    ui_status_dot = lv_obj_create(left);
    lv_obj_set_size(ui_status_dot, 12, 12);
    lv_obj_set_style_bg_color(ui_status_dot, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_opa(ui_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui_status_dot, 0, 0);
    lv_obj_clear_flag(ui_status_dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(left);
    lv_label_set_text(title, "CHARPY TESTER");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* Center: state badge */
    ui_status_state_label = lv_label_create(bar);
    lv_label_set_text(ui_status_state_label, "IDLE");
    lv_obj_set_style_text_color(ui_status_state_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_status_state_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(ui_status_state_label, UI_COLOR_SURFACE_EL, 0);
    lv_obj_set_style_bg_opa(ui_status_state_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(ui_status_state_label, 12, 0);
    lv_obj_set_style_pad_ver(ui_status_state_label, 4, 0);
    lv_obj_set_style_radius(ui_status_state_label, 10, 0);

    /* Right: time + date + SD — fixed width so flex layout never clips it */
    lv_obj_t *right = lv_obj_create(bar);
    lv_obj_set_size(right, 300, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 12, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    ui_status_time_label = lv_label_create(right);
    lv_label_set_text(ui_status_time_label, "00:00:00");
    lv_obj_set_style_text_color(ui_status_time_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui_status_time_label, &lv_font_montserrat_16, 0);

    ui_status_date_label = lv_label_create(right);
    lv_label_set_text(ui_status_date_label, "01-Jan-2026");
    lv_obj_set_style_text_color(ui_status_date_label, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(ui_status_date_label, &lv_font_montserrat_16, 0);

    ui_status_sd_label = lv_label_create(right);
    lv_label_set_text(ui_status_sd_label, LV_SYMBOL_SD_CARD " " LV_SYMBOL_OK);
    lv_obj_set_style_text_color(ui_status_sd_label, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(ui_status_sd_label, &lv_font_montserrat_16, 0);
}

void ui_init(void)
{
    /* Init theme as dark */
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        dispp, UI_COLOR_PRIMARY, UI_COLOR_DANGER, true, &lv_font_montserrat_16);
    lv_display_set_theme(dispp, theme);

    /* Init shared styles */
    init_styles();

    /* Create all screens */
    ui_dashboard_create();
    ui_specimen_create();
    ui_test_active_create();
    ui_history_create();
    ui_settings_create();
    ui_dbtt_setup_create();
    ui_dbtt_run_create();
    ui_dbtt_result_create();
    ui_dbtt_history_create();

    /*
     * Create the status bar ONCE on the LVGL top layer so it is always
     * visible above every screen.  The global widget pointers
     * (ui_status_time_label, etc.) are set here and never overwritten.
     *
     * lv_layer_top() inherits the active theme's default padding — clear it
     * so the bar sits flush at (0,0) and is not clipped.
     */
    lv_obj_t *top_layer = lv_layer_top();
    lv_obj_set_style_pad_all(top_layer, 0, 0);
    lv_obj_set_style_border_width(top_layer, 0, 0);
    lv_obj_clear_flag(top_layer, LV_OBJ_FLAG_SCROLLABLE);
    create_status_bar(top_layer);

    /* Show dashboard by default */
    ui_show_dashboard();
}

void ui_show_dashboard(void)
{
    lv_scr_load_anim(scr_dashboard, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_specimen(void)
{
    lv_scr_load_anim(scr_specimen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_test_active(void)
{
    lv_scr_load_anim(scr_test_active, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_history(void)
{
    ui_history_refresh();
    lv_scr_load_anim(scr_history, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_settings(void)
{
    lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

/* DBTT navigation */
void ui_show_dbtt(void)
{
    lv_scr_load_anim(scr_dbtt_setup, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_dbtt_run(void)
{
    ui_dbtt_run_refresh();
    lv_scr_load_anim(scr_dbtt_run, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_dbtt_result(void)
{
    ui_dbtt_result_refresh();
    lv_scr_load_anim(scr_dbtt_result, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_dbtt_history(void)
{
    ui_dbtt_history_refresh();
    lv_scr_load_anim(scr_dbtt_history, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_show_dbtt_result_history(void)
{
    /* ui_dbtt_result_load() was already called — just navigate */
    lv_scr_load_anim(scr_dbtt_result, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void ui_update_status_bar(const char *time_str, const char *date_str,
                          const char *state_str, lv_color_t dot_color, bool sd_ok)
{
    if (ui_status_time_label)  lv_label_set_text(ui_status_time_label, time_str);
    if (ui_status_date_label)  lv_label_set_text(ui_status_date_label, date_str);
    if (ui_status_state_label) lv_label_set_text(ui_status_state_label, state_str);
    if (ui_status_dot)         lv_obj_set_style_bg_color(ui_status_dot, dot_color, 0);
    if (ui_status_sd_label) {
        if (sd_ok) {
            lv_label_set_text(ui_status_sd_label, LV_SYMBOL_SD_CARD " " LV_SYMBOL_OK);
            lv_obj_set_style_text_color(ui_status_sd_label, UI_COLOR_SUCCESS, 0);
        } else {
            lv_label_set_text(ui_status_sd_label, LV_SYMBOL_SD_CARD " " LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(ui_status_sd_label, UI_COLOR_DANGER, 0);
        }
    }
}

/* Status bar lives on lv_layer_top() — this shim keeps existing screen
 * constructors compilable without creating duplicate bars. */
void ui_create_screen_status_bar(lv_obj_t *screen)
{
    (void)screen;
}
