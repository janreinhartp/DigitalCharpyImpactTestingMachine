#ifndef _UI_H_
#define _UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

/*——————————————— Dark Theme Colors ———————————————*/
#define UI_COLOR_BG           lv_color_hex(0x1A1A2E)
#define UI_COLOR_SURFACE      lv_color_hex(0x16213E)
#define UI_COLOR_SURFACE_EL   lv_color_hex(0x0F3460)
#define UI_COLOR_PRIMARY      lv_color_hex(0x00D4FF)
#define UI_COLOR_DANGER       lv_color_hex(0xE94560)
#define UI_COLOR_SUCCESS      lv_color_hex(0x00E676)
#define UI_COLOR_WARNING      lv_color_hex(0xFFC107)
#define UI_COLOR_TEXT         lv_color_hex(0xEAEAEA)
#define UI_COLOR_TEXT_SEC     lv_color_hex(0x8892A0)
#define UI_COLOR_TEXT_MUTED   lv_color_hex(0x4A5568)
#define UI_COLOR_BORDER       lv_color_hex(0x2D3748)

/*——————————————— Status Bar Height ———————————————*/
#define UI_STATUS_BAR_H  40

/*——————————————— Shared Styles ———————————————*/
extern lv_style_t style_card;
extern lv_style_t style_btn_primary;
extern lv_style_t style_btn_secondary;
extern lv_style_t style_btn_danger;
extern lv_style_t style_btn_warning;
extern lv_style_t style_input;

/*——————————————— Screen Objects ———————————————*/
extern lv_obj_t *scr_dashboard;
extern lv_obj_t *scr_specimen;
extern lv_obj_t *scr_test_active;
extern lv_obj_t *scr_history;
extern lv_obj_t *scr_settings;
extern lv_obj_t *scr_dbtt_setup;
extern lv_obj_t *scr_dbtt_run;
extern lv_obj_t *scr_dbtt_result;
extern lv_obj_t *scr_dbtt_history;

/*——————————————— Status Bar Labels (updated from main loop) ————*/
extern lv_obj_t *ui_status_time_label;
extern lv_obj_t *ui_status_date_label;
extern lv_obj_t *ui_status_state_label;
extern lv_obj_t *ui_status_dot;
extern lv_obj_t *ui_status_sd_label;

/*——————————————— Dashboard Widgets ———————————————*/
extern lv_obj_t *ui_dash_arc;
extern lv_obj_t *ui_dash_angle_label;
extern lv_obj_t *ui_dash_last_angle_label;
extern lv_obj_t *ui_dash_last_energy_label;
extern lv_obj_t *ui_dash_last_specimen_label;
extern lv_obj_t *ui_dash_last_material_label;
extern lv_obj_t *ui_dash_last_operator_label;
extern lv_obj_t *ui_dash_last_timestamp_label;

/*——————————————— Specimen Entry Widgets ———————————————*/
extern lv_obj_t *ui_spec_id_ta;
extern lv_obj_t *ui_spec_operator_ta;
extern lv_obj_t *ui_spec_material_dd;
extern lv_obj_t *ui_spec_width_ta;
extern lv_obj_t *ui_spec_height_ta;
extern lv_obj_t *ui_spec_length_ta;
extern lv_obj_t *ui_spec_temp_ta;
extern lv_obj_t *ui_spec_notes_ta;
extern lv_obj_t *ui_spec_keyboard;

/*——————————————— Test Active Widgets ———————————————*/
extern lv_obj_t *ui_test_arc;
extern lv_obj_t *ui_test_angle_label;
extern lv_obj_t *ui_test_state_label;
extern lv_obj_t *ui_test_detail_label;   /* homing sub-step description */
extern lv_obj_t *ui_test_progress_dots[4];
extern lv_obj_t *ui_test_result_angle_label;
extern lv_obj_t *ui_test_result_energy_label;
extern lv_obj_t *ui_test_btn_release;
extern lv_obj_t *ui_test_btn_abort;
extern lv_obj_t *ui_test_btn_save;
extern lv_obj_t *ui_test_btn_discard;
extern lv_obj_t *ui_test_result_panel;

/*——————————————— History Widgets ———————————————*/
extern lv_obj_t *ui_hist_table;
extern lv_obj_t *ui_hist_count;
extern lv_obj_t *ui_hist_page;

/*——————————————— DBTT Widgets ———————————————*/
/* (widget handles are static within their respective .c files) */

/*——————————————— Settings Widgets ———————————————*/
extern lv_obj_t *ui_set_mass_ta;
extern lv_obj_t *ui_set_length_ta;
extern lv_obj_t *ui_set_release_ta;
extern lv_obj_t *ui_set_zero_label;
extern lv_obj_t *ui_set_audio_sw;
extern lv_obj_t *ui_set_sd_label;
extern lv_obj_t *ui_set_rtc_label;
extern lv_obj_t *ui_set_about_label;

/*——————————————— Init / Navigation ———————————————*/
void ui_init(void);
void ui_show_dashboard(void);
void ui_show_specimen(void);
void ui_show_test_active(void);
void ui_show_history(void);
void ui_show_settings(void);
void ui_show_dbtt(void);                  /* -> scr_dbtt_setup */
void ui_show_dbtt_run(void);              /* -> scr_dbtt_run    (refresh + load) */
void ui_show_dbtt_result(void);           /* -> scr_dbtt_result (refresh + load) */
void ui_show_dbtt_history(void);          /* -> scr_dbtt_history (refresh + load) */
void ui_show_dbtt_result_history(void);   /* -> scr_dbtt_result  (history mode)   */

/*——————————————— Screen Constructors ———————————————*/
void ui_dashboard_create(void);
void ui_specimen_create(void);
void ui_test_active_create(void);
void ui_history_create(void);
void ui_settings_create(void);
void ui_dbtt_setup_create(void);
void ui_dbtt_run_create(void);
void ui_dbtt_result_create(void);
void ui_dbtt_history_create(void);
void ui_create_screen_status_bar(lv_obj_t *screen);

/*——————————————— UI Update Helpers ———————————————*/
void ui_update_status_bar(const char *time_str, const char *date_str,
                          const char *state_str, lv_color_t dot_color, bool sd_ok);
void ui_test_active_set_state(int state);
void ui_history_refresh(void);
void ui_dbtt_run_refresh(void);
void ui_dbtt_result_refresh(void);
void ui_dbtt_history_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
