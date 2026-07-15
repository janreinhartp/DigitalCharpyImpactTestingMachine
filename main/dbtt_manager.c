#include "dbtt_manager.h"
#include "bsp_sdcard.h"
#include "bsp_rtc.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>

static const char *TAG = "dbtt_mgr";

static dbtt_session_t s_session;
static bool           s_active = false;

/* ——————————————— Public API ——————————————— */

void dbtt_manager_start(const char *material, const char *operator_name,
                        int n_planned,
                        float width_mm, float height_mm, float length_mm)
{
    memset(&s_session, 0, sizeof(s_session));
    strncpy(s_session.material,      material,      sizeof(s_session.material) - 1);
    strncpy(s_session.operator_name, operator_name, sizeof(s_session.operator_name) - 1);
    s_session.n_planned = (n_planned > DBTT_SESSION_MAX_TESTS)
                          ? DBTT_SESSION_MAX_TESTS : n_planned;
    s_session.n_done    = 0;
    s_session.width_mm  = width_mm;
    s_session.height_mm = height_mm;
    s_session.length_mm = length_mm;
    s_active = true;
    ESP_LOGI(TAG, "Session started: %s, N=%d", material, s_session.n_planned);
}

void dbtt_manager_reset(void)
{
    memset(&s_session, 0, sizeof(s_session));
    s_active = false;
    ESP_LOGI(TAG, "Session reset");
}

bool dbtt_manager_is_active(void)
{
    return s_active;
}

void dbtt_manager_record_result(const test_result_t *result)
{
    if (!result || !s_active || s_session.n_done >= DBTT_SESSION_MAX_TESTS) return;

    dbtt_point_t *p    = &s_session.points[s_session.n_done];
    p->temperature_c   = result->specimen.temperature_c;
    p->energy_joules   = result->energy_joules;
    p->final_angle_deg = result->final_angle_deg;
    strncpy(p->specimen_id, result->specimen.specimen_id, sizeof(p->specimen_id) - 1);
    s_session.n_done++;

    ESP_LOGI(TAG, "Test %d/%d recorded: T=%.1f°C  E=%.2f J",
             s_session.n_done, s_session.n_planned,
             (double)p->temperature_c, (double)p->energy_joules);

    if (s_session.n_done >= s_session.n_planned) {
        s_active = false;   /* all planned tests complete */
        ESP_LOGI(TAG, "Session complete");
        dbtt_manager_save_session();   /* auto-save binary to SD */
    }
}

const dbtt_session_t *dbtt_manager_get_session(void)
{
    return &s_session;
}

esp_err_t dbtt_manager_save_csv(void)
{
    if (!sdcard_is_mounted()) {
        ESP_LOGW(TAG, "SD card not mounted — cannot save");
        return ESP_ERR_INVALID_STATE;
    }

    FILE *f = fopen("/sdcard/DBTT_results.csv", "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open /sdcard/DBTT_results.csv");
        return ESP_FAIL;
    }

    /* Session header block */
    fprintf(f, "\n### DBTT Session | Material: %s | Operator: %s | N=%d\n",
            s_session.material, s_session.operator_name, s_session.n_done);
    fprintf(f, "Test#,Temperature_C,Final_Angle_deg,Energy_J,Specimen_ID\n");

    for (int i = 0; i < s_session.n_done; i++) {
        const dbtt_point_t *p = &s_session.points[i];
        fprintf(f, "%d,%.1f,%.1f,%.2f,%s\n",
                i + 1,
                (double)p->temperature_c,
                (double)p->final_angle_deg,
                (double)p->energy_joules,
                p->specimen_id);
    }

    fclose(f);
    ESP_LOGI(TAG, "Saved %d records to /sdcard/DBTT_results.csv", s_session.n_done);
    return ESP_OK;
}

/* ——————————————— Binary session persistence ——————————————— */

esp_err_t dbtt_manager_save_session(void)
{
    if (!sdcard_is_mounted()) {
        ESP_LOGW(TAG, "SD not mounted — skipping binary save");
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[64];
    rtc_datetime_t dt;
    if (rtc_get_datetime(&dt) == ESP_OK) {
        snprintf(filepath, sizeof(filepath),
                 "/sdcard/DBTT_%04d%02d%02d_%02d%02d%02d.dat",
                 dt.year, dt.month, dt.date,
                 dt.hours, dt.minutes, dt.seconds);
    } else {
        static int s_seq = 0;
        snprintf(filepath, sizeof(filepath), "/sdcard/DBTT_session_%03d.dat", s_seq++);
    }

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot create %s", filepath);
        return ESP_FAIL;
    }
    size_t written = fwrite(&s_session, sizeof(s_session), 1, f);
    fclose(f);

    if (written != 1) {
        ESP_LOGE(TAG, "Partial write to %s", filepath);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Session saved to %s", filepath);
    return ESP_OK;
}

int dbtt_manager_list_sessions(char list[][32], int max)
{
    if (!sdcard_is_mounted() || max <= 0) return 0;

    DIR *dir = opendir("/sdcard");
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (strncmp(name, "DBTT_", 5) == 0 && len > 4 &&
            strcmp(name + len - 4, ".dat") == 0) {
            strncpy(list[count], name, 31);
            list[count][31] = '\0';
            count++;
        }
    }
    closedir(dir);

    /* Bubble-sort alphabetically (== chronologically with our naming) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(list[i], list[j]) > 0) {
                char tmp[32];
                memcpy(tmp,       list[i], 32);
                memcpy(list[i],   list[j], 32);
                memcpy(list[j],   tmp,     32);
            }
        }
    }
    return count;
}

esp_err_t dbtt_manager_load_session(const char *filename, dbtt_session_t *out)
{
    if (!filename || !out) return ESP_ERR_INVALID_ARG;

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/%s", filename);

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s", path);
        return ESP_FAIL;
    }
    size_t n = fread(out, sizeof(dbtt_session_t), 1, f);
    fclose(f);
    return (n == 1) ? ESP_OK : ESP_FAIL;
}
