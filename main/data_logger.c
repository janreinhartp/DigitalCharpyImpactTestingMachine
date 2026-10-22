#include "data_logger.h"
#include "bsp_sdcard.h"
#include "bsp_rtc.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define DL_TAG "DATA_LOG"
#define DL_INFO(fmt, ...) ESP_LOGI(DL_TAG, fmt, ##__VA_ARGS__)
#define DL_ERROR(fmt, ...) ESP_LOGE(DL_TAG, fmt, ##__VA_ARGS__)

#define CSV_HEADER "timestamp,specimen_id,material,width_mm,height_mm,length_mm,temperature_c,operator,release_angle_deg,final_angle_deg,mass_kg,arm_length_m,energy_joules,notes"

static test_result_t history_cache[HISTORY_CACHE_SIZE];
static int history_count = 0;

typedef struct {
    bool first_line;
    bool has_current_schema;
} csv_schema_state_t;

static void read_csv_schema(const char *line, void *user_data)
{
    csv_schema_state_t *state = user_data;
    if (state->first_line) {
        state->has_current_schema =
            strstr(line, ",mass_kg,") != NULL &&
            strstr(line, ",arm_length_m,") != NULL;
        state->first_line = false;
    }
}

static esp_err_t use_versioned_csv_if_needed(char *filepath, size_t filepath_size)
{
    if (!sdcard_file_exists(filepath)) {
        return ESP_OK;
    }

    csv_schema_state_t state = {
        .first_line = true,
        .has_current_schema = false,
    };
    esp_err_t err = sdcard_read_lines(filepath, read_csv_schema, &state);
    if (err != ESP_OK) {
        DL_ERROR("Failed to check CSV schema for %s", filepath);
        return err;
    }

    if (!state.first_line && !state.has_current_schema) {
        size_t path_len = strlen(filepath);
        if (path_len < 4 || strcmp(filepath + path_len - 4, ".csv") != 0) {
            DL_ERROR("Cannot version unexpected CSV path: %s", filepath);
            return ESP_ERR_INVALID_ARG;
        }

        char original_path[64];
        if (path_len >= sizeof(original_path)) {
            DL_ERROR("CSV path is too long to version");
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(original_path, filepath, path_len + 1);
        int written = snprintf(filepath, filepath_size, "%.*s_V2.csv",
                               (int)(path_len - 4), original_path);
        if (written < 0 || (size_t)written >= filepath_size) {
            DL_ERROR("Versioned CSV path is too long");
            return ESP_ERR_INVALID_SIZE;
        }
    }

    return ESP_OK;
}

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
 * @brief Build the daily CSV filepath: /sdcard/CHARPY_YYYYMMDD.csv
 */
static void build_filepath(char *buf, size_t buf_size)
{
    rtc_datetime_t dt;
    if (rtc_get_datetime(&dt) == ESP_OK) {
        char date_str[12];
        rtc_format_date_compact(&dt, date_str, sizeof(date_str));
        snprintf(buf, buf_size, "/sdcard/CHARPY_%s.csv", date_str);
    } else {
        /* Fallback if RTC fails */
        snprintf(buf, buf_size, "/sdcard/CHARPY_UNKNOWN.csv");
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
    build_filepath(filepath, sizeof(filepath));
    esp_err_t err = use_versioned_csv_if_needed(filepath, sizeof(filepath));
    if (err != ESP_OK) {
        return err;
    }

    /* Ensure file exists with header */
    err = sdcard_ensure_file_with_header(filepath, CSV_HEADER);
    if (err != ESP_OK)
        return err;

    /* Format CSV line */
    char line[256];
    snprintf(line, sizeof(line),
             "%s,%s,%s,%.1f,%.1f,%.1f,%.1f,%s,%.1f,%.1f,%.3f,%.3f,%.2f,%s",
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
             result->mass_kg,
             result->arm_length_m,
             result->energy_joules,
             result->specimen.notes);

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

    test_result_t *r = &history_cache[history_count];
    memset(r, 0, sizeof(test_result_t));

    /* Parse the current CSV schema; fall back to rows written by older firmware. */
    int parsed = sscanf(line,
        "%23[^,],%31[^,],%31[^,],%f,%f,%f,%f,%31[^,],%f,%f,%f,%f,%f,%63[^\n]",
        r->timestamp,
        r->specimen.specimen_id,
        r->specimen.material,
        &r->specimen.width_mm,
        &r->specimen.height_mm,
        &r->specimen.length_mm,
        &r->specimen.temperature_c,
        r->specimen.operator_name,
        &r->release_angle_deg,
        &r->final_angle_deg,
        &r->mass_kg,
        &r->arm_length_m,
        &r->energy_joules,
        r->specimen.notes);

    if (parsed < 13) {
        memset(r, 0, sizeof(test_result_t));
        parsed = sscanf(line,
        "%23[^,],%31[^,],%31[^,],%f,%f,%f,%f,%31[^,],%f,%f,%f,%63[^\n]",
        r->timestamp,
        r->specimen.specimen_id,
        r->specimen.material,
        &r->specimen.width_mm,
        &r->specimen.height_mm,
        &r->specimen.length_mm,
        &r->specimen.temperature_c,
        r->specimen.operator_name,
        &r->release_angle_deg,
        &r->final_angle_deg,
        &r->energy_joules,
        r->specimen.notes);
    }

    if (parsed >= 11) {
        history_count++;
    }
}

esp_err_t data_logger_load_history(void)
{
    if (!sdcard_is_mounted())
        return ESP_ERR_INVALID_STATE;

    history_count = 0;

    /* Load only today's file — one file per day */
    char filepath[64];
    build_filepath(filepath, sizeof(filepath));

    if (sdcard_file_exists(filepath)) {
        DL_INFO("Loading today's history from %s", filepath);
        esp_err_t err = sdcard_read_lines(filepath, parse_csv_line, NULL);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        DL_INFO("No history file for today (%s)", filepath);
    }

    size_t path_len = strlen(filepath);
    if (path_len >= 4 && strcmp(filepath + path_len - 4, ".csv") == 0) {
        char versioned_filepath[64];
        int written = snprintf(versioned_filepath, sizeof(versioned_filepath),
                               "%.*s_V2.csv", (int)(path_len - 4), filepath);
        if (written < 0 || (size_t)written >= sizeof(versioned_filepath)) {
            DL_ERROR("Versioned CSV path is too long");
            return ESP_ERR_INVALID_SIZE;
        }
        if (sdcard_file_exists(versioned_filepath)) {
            DL_INFO("Loading additional history from %s", versioned_filepath);
            esp_err_t err = sdcard_read_lines(versioned_filepath, parse_csv_line, NULL);
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    DL_INFO("Loaded %d result(s) from today's file", history_count);
    return ESP_OK;
}

int data_logger_get_record_count(void)
{
    return history_count;
}
