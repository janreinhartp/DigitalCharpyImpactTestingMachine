#include "data_logger.h"
#include "bsp_sdcard.h"
#include "bsp_rtc.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DL_TAG "DATA_LOG"
#define DL_INFO(fmt, ...) ESP_LOGI(DL_TAG, fmt, ##__VA_ARGS__)
#define DL_ERROR(fmt, ...) ESP_LOGE(DL_TAG, fmt, ##__VA_ARGS__)

#define CSV_HEADER "timestamp,specimen_id,material,width_mm,height_mm,length_mm,temperature_c,operator,release_angle,final_angle,energy_joules,notes,notch_depth_mm,impact_strength_j_cm2"

static test_result_t history_cache[HISTORY_CACHE_SIZE];
static int history_count = 0;

esp_err_t data_logger_init(void)
{
    if (!sdcard_is_mounted()) {
        DL_ERROR("SD card not mounted — data logger unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    DL_INFO("Data logger initialized");
    return ESP_OK;
}

/**
 * @brief Build the legacy or current daily CSV filepath
 */
static void build_filepath(char *buf, size_t buf_size, bool versioned)
{
    rtc_datetime_t dt;
    if (rtc_get_datetime(&dt) == ESP_OK) {
        char date_str[12];
        rtc_format_date_compact(&dt, date_str, sizeof(date_str));
        snprintf(buf, buf_size, versioned ? "/sdcard/CHARPY_%s_V2.csv"
                          : "/sdcard/CHARPY_%s.csv",
             date_str);
    } else {
        /* Fallback if RTC fails */
        snprintf(buf, buf_size, versioned ? "/sdcard/CHARPY_UNKNOWN_V2.csv"
                          : "/sdcard/CHARPY_UNKNOWN.csv");
    }
}

esp_err_t data_logger_write_result(const test_result_t *result)
{
    if (result == NULL)
        return ESP_ERR_INVALID_ARG;

    /* Always add to in-memory cache regardless of SD card state */
    if (history_count < HISTORY_CACHE_SIZE) {
        history_cache[history_count] = *result;
        history_count++;
    } else {
        /* Shift array left by 1, drop oldest */
        memmove(&history_cache[0], &history_cache[1],
                (HISTORY_CACHE_SIZE - 1) * sizeof(test_result_t));
        history_cache[HISTORY_CACHE_SIZE - 1] = *result;
    }

    /* Write to SD card if available */
    if (!sdcard_is_mounted()) {
        DL_ERROR("SD card not mounted — result cached in RAM only (%d cached)", history_count);
        return ESP_ERR_INVALID_STATE;
    }

    char filepath[64];
    build_filepath(filepath, sizeof(filepath), true);

    /* Ensure file exists with header */
    esp_err_t err = sdcard_ensure_file_with_header(filepath, CSV_HEADER);
    if (err != ESP_OK)
        return err;

    /* Format CSV line */
    char line[320];
    snprintf(line, sizeof(line),
             "%s,%s,%s,%.1f,%.1f,%.1f,%.1f,%s,%.1f,%.1f,%.2f,%s,%.2f,%.2f",
             result->timestamp,
             result->specimen.specimen_id,
             result->specimen.material,
             result->specimen.width_mm,
             result->specimen.height_mm,
             result->specimen.length_mm,
             result->specimen.temperature_c,
             result->specimen.operator_name,
             result->release_angle_deg,
             result->final_angle_deg,
             result->energy_joules,
             result->specimen.notes,
             result->specimen.notch_depth_mm,
             result->impact_strength_j_cm2);

    err = sdcard_append_line(filepath, line);
    if (err != ESP_OK) {
        DL_ERROR("Failed to write to %s", filepath);
        return err;
    }

    DL_INFO("Result logged to %s (%d cached)", filepath, history_count);
    return ESP_OK;
}

esp_err_t data_logger_get_history(const test_result_t **results, int *count)
{
    if (results == NULL || count == NULL)
        return ESP_ERR_INVALID_ARG;

    *results = history_cache;
    *count = history_count;
    return ESP_OK;
}

/* Callback for parsing CSV lines into history cache */
static void parse_csv_line(const char *line, void *user_data)
{
    (void)user_data;

    /* Skip header line */
    if (strncmp(line, "timestamp", 9) == 0)
        return;

    if (history_count >= HISTORY_CACHE_SIZE) {
        /* Evict the oldest record to make room — keeps the cache as the
         * HISTORY_CACHE_SIZE most-recent results in chronological order. */
        memmove(&history_cache[0], &history_cache[1],
                (HISTORY_CACHE_SIZE - 1) * sizeof(test_result_t));
        history_count = HISTORY_CACHE_SIZE - 1;
    }

    char copy[512];
    snprintf(copy, sizeof(copy), "%s", line);

    char *fields[14] = {0};
    int field_count = 1;
    fields[0] = copy;
    for (char *cursor = copy; *cursor != '\0' && field_count < 14; cursor++) {
        if (*cursor == ',') {
            *cursor = '\0';
            fields[field_count++] = cursor + 1;
        }
    }
    if (field_count < 11) return;

    test_result_t *r = &history_cache[history_count];
    memset(r, 0, sizeof(test_result_t));

    snprintf(r->timestamp, sizeof(r->timestamp), "%s", fields[0]);
    snprintf(r->specimen.specimen_id, sizeof(r->specimen.specimen_id), "%s", fields[1]);
    snprintf(r->specimen.material, sizeof(r->specimen.material), "%s", fields[2]);
    r->specimen.width_mm = strtof(fields[3], NULL);
    r->specimen.height_mm = strtof(fields[4], NULL);
    r->specimen.length_mm = strtof(fields[5], NULL);
    r->specimen.temperature_c = strtof(fields[6], NULL);
    snprintf(r->specimen.operator_name, sizeof(r->specimen.operator_name), "%s", fields[7]);
    r->release_angle_deg = strtof(fields[8], NULL);
    r->final_angle_deg = strtof(fields[9], NULL);
    r->energy_joules = strtof(fields[10], NULL);
    if (field_count >= 12) {
        snprintf(r->specimen.notes, sizeof(r->specimen.notes), "%s", fields[11]);
    }
    if (field_count >= 14) {
        r->specimen.notch_depth_mm = strtof(fields[12], NULL);
        r->impact_strength_j_cm2 = strtof(fields[13], NULL);
        float area_cm2;
        r->impact_strength_valid =
            charpy_calc_net_area_cm2(r->specimen.width_mm,
                                     r->specimen.height_mm,
                                     r->specimen.notch_depth_mm,
                                     &area_cm2) &&
            isfinite(r->impact_strength_j_cm2) &&
            r->impact_strength_j_cm2 >= 0.0f;
    }
    history_count++;
}

esp_err_t data_logger_load_history(void)
{
    if (!sdcard_is_mounted())
        return ESP_ERR_INVALID_STATE;

    history_count = 0;

    /* Load today's legacy file first, then the versioned file. */
    char filepath[64];
    build_filepath(filepath, sizeof(filepath), false);
    if (sdcard_file_exists(filepath)) {
        DL_INFO("Loading legacy history from %s", filepath);
        sdcard_read_lines(filepath, parse_csv_line, NULL);
    }

    build_filepath(filepath, sizeof(filepath), true);
    if (sdcard_file_exists(filepath)) {
        DL_INFO("Loading current history from %s", filepath);
        sdcard_read_lines(filepath, parse_csv_line, NULL);
    }
    DL_INFO("Loaded %d result(s) from today's file", history_count);
    return ESP_OK;
}

int data_logger_get_record_count(void)
{
    return history_count;
}
