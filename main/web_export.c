#include "web_export.h"

#include "bsp_sdcard.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define WE_TAG "WEB_EXPORT"
#define WE_INFO(fmt, ...) ESP_LOGI(WE_TAG, fmt, ##__VA_ARGS__)
#define WE_ERROR(fmt, ...) ESP_LOGE(WE_TAG, fmt, ##__VA_ARGS__)

#define WEB_MAX_FILES 48
#define WEB_NAME_LEN  40
#define WEB_CHUNK     1024

typedef struct {
    char name[WEB_NAME_LEN];
    uint64_t size;
} web_file_t;

static const char WEB_PAGE_HEAD[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Charpy test data</title><style>"
    "body{font-family:sans-serif;background:#1a1a2e;color:#eaeaea;margin:24px}"
    "a{color:#00d4ff}table{border-collapse:collapse;width:100%;max-width:720px}"
    "td,th{border-bottom:1px solid #2d3748;text-align:left;padding:8px 12px 8px 0}"
    "</style></head><body><h1>Charpy test data</h1>"
    "<p>Save a CSV file to this computer. The SD card stays in the machine.</p>";

static const char WEB_PAGE_TAIL[] = "</body></html>";

static httpd_handle_t s_server = NULL;

static bool csv_export_allowed(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    size_t len = strlen(name);
    if (len >= WEB_NAME_LEN || strstr(name, "..") != NULL) {
        return false;
    }

    for (const char *p = name; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '_' || c == '.')) {
            return false;
        }
    }

    if (strcmp(name, "DBTT_results.csv") == 0) {
        return true;
    }

    if (strncmp(name, "CHARPY_", 7) != 0 || len < 5) {
        return false;
    }
    return strcmp(name + len - 4, ".csv") == 0;
}

static int collect_csv_files(web_file_t *files, int max_files)
{
    int count = 0;

    sdcard_lock();
    DIR *dir = opendir("/sdcard");
    if (dir == NULL) {
        sdcard_unlock();
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        char name[WEB_NAME_LEN];
        strncpy(name, entry->d_name, WEB_NAME_LEN - 1);
        name[WEB_NAME_LEN - 1] = '\0';
        if (!csv_export_allowed(name)) {
            continue;
        }

        char path[64];
        snprintf(path, sizeof(path), "/sdcard/%s", name);
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }

        memcpy(files[count].name, name, WEB_NAME_LEN);
        files[count].size = (uint64_t)st.st_size;
        count++;
    }
    closedir(dir);
    sdcard_unlock();

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(files[i].name, files[j].name) < 0) {
                web_file_t tmp = files[i];
                files[i] = files[j];
                files[j] = tmp;
            }
        }
    }
    return count;
}

static esp_err_t send_text_chunk(httpd_req_t *req, const char *text)
{
    return httpd_resp_send_chunk(req, text, strlen(text));
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    esp_err_t err = send_text_chunk(req, WEB_PAGE_HEAD);
    if (err != ESP_OK) {
        return err;
    }

    if (!sdcard_is_mounted()) {
        err = send_text_chunk(req, "<p>SD card is not mounted. Insert the card and restart.</p>");
    } else {
        web_file_t files[WEB_MAX_FILES];
        int count = collect_csv_files(files, WEB_MAX_FILES);
        if (count == 0) {
            err = send_text_chunk(req, "<p>No CSV files on the card yet.</p>");
        } else {
            err = send_text_chunk(req, "<table><tr><th>File</th><th>Size</th><th></th></tr>");
            for (int i = 0; i < count && err == ESP_OK; i++) {
                char row[320];
                snprintf(row, sizeof(row),
                         "<tr><td>%s</td><td>%llu bytes</td>"
                         "<td><a href=\"/download?name=%s\">Save</a></td></tr>",
                         files[i].name,
                         (unsigned long long)files[i].size,
                         files[i].name);
                err = send_text_chunk(req, row);
            }
            if (err == ESP_OK) {
                err = send_text_chunk(req, "</table>");
            }
        }
    }

    if (err == ESP_OK) {
        err = send_text_chunk(req, WEB_PAGE_TAIL);
    }
    if (err == ESP_OK) {
        err = httpd_resp_send_chunk(req, NULL, 0);
    }
    return err;
}

static esp_err_t download_get_handler(httpd_req_t *req)
{
    char query[128];
    char name[WEB_NAME_LEN];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "name", name, sizeof(name)) != ESP_OK ||
        !csv_export_allowed(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File not allowed");
        return ESP_OK;
    }

    if (!sdcard_is_mounted()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card is not mounted");
        return ESP_OK;
    }

    char path[8 + WEB_NAME_LEN];
    snprintf(path, sizeof(path), "/sdcard/%s", name);

    sdcard_lock();
    FILE *f = fopen(path, "rb");
    sdcard_unlock();
    if (f == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_OK;
    }

    char disposition[32 + WEB_NAME_LEN];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", name);
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);

    char buf[WEB_CHUNK];
    esp_err_t err = ESP_OK;
    while (err == ESP_OK) {
        sdcard_lock();
        size_t n = fread(buf, 1, sizeof(buf), f);
        int read_error = ferror(f);
        sdcard_unlock();

        if (read_error) {
            WE_ERROR("Read failed for %s", name);
            err = ESP_FAIL;
            break;
        }
        if (n == 0) {
            break;
        }
        err = httpd_resp_send_chunk(req, buf, n);
    }

    sdcard_lock();
    fclose(f);
    sdcard_unlock();

    if (err == ESP_OK) {
        err = httpd_resp_send_chunk(req, NULL, 0);
    }
    return err;
}

esp_err_t web_export_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 12288;
    config.max_uri_handlers = 4;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        WE_ERROR("HTTP server start failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    const httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL,
    };
    const httpd_uri_t download_uri = {
        .uri = "/download",
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(s_server, &index_uri);
    httpd_register_uri_handler(s_server, &download_uri);
    WE_INFO("CSV download page at http://192.168.4.1/");
    return ESP_OK;
}

esp_err_t web_export_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t err = httpd_stop(s_server);
    s_server = NULL;
    if (err != ESP_OK) {
        WE_ERROR("HTTP server stop failed: %s", esp_err_to_name(err));
        return err;
    }
    WE_INFO("CSV download page stopped");
    return ESP_OK;
}
