#include "wifi_portal.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include <string.h>

#define WP_TAG "WIFI_PORTAL"
#define WP_INFO(fmt, ...) ESP_LOGI(WP_TAG, fmt, ##__VA_ARGS__)
#define WP_ERROR(fmt, ...) ESP_LOGE(WP_TAG, fmt, ##__VA_ARGS__)

#define WIFI_PORTAL_CHANNEL 1
#define WIFI_PORTAL_MAX_CONN 4

static bool s_started = false;
static bool s_wifi_inited = false;
static esp_netif_t *s_ap_netif = NULL;
static esp_event_handler_instance_t s_wifi_handler = NULL;

bool wifi_portal_is_started(void)
{
    return s_started;
}

/* Drop the AP, the hosted SDIO driver, and the netif so a later start is clean. */
static void wifi_portal_teardown(void)
{
    if (s_started) {
        esp_wifi_stop();
        s_started = false;
    }
    if (s_wifi_handler != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_handler);
        s_wifi_handler = NULL;
    }
    if (s_wifi_inited) {
        esp_wifi_deinit();
        s_wifi_inited = false;
    }
    if (s_ap_netif != NULL) {
        esp_wifi_clear_default_wifi_driver_and_handlers(s_ap_netif);
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }
}

esp_err_t wifi_portal_stop(void)
{
    wifi_portal_teardown();
    WP_INFO("Hotspot stopped");
    return ESP_OK;
}

/* Same join/leave log as Elecrow Lesson 17 V1.2 softAP. */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        WP_INFO("station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        WP_INFO("station " MACSTR " leave, AID=%d, reason=%d",
                MAC2STR(event->mac), event->aid, event->reason);
    }
}

esp_err_t wifi_portal_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    /* Order follows ESP32_P4-softAP. The default AP address is 192.168.4.1. */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        WP_ERROR("netif init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        WP_ERROR("event loop failed: %s", esp_err_to_name(err));
        return err;
    }

    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) {
            WP_ERROR("AP netif create failed");
            wifi_portal_teardown();
            return ESP_FAIL;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        WP_ERROR("Wi-Fi init failed: %s", esp_err_to_name(err));
        WP_ERROR("The ESP32-C6 must run ESP-Hosted SDIO slave firmware that matches this build");
        wifi_portal_teardown();
        return err;
    }
    s_wifi_inited = true;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, &s_wifi_handler);
    if (err != ESP_OK) {
        WP_ERROR("Wi-Fi event register failed: %s", esp_err_to_name(err));
        wifi_portal_teardown();
        return err;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.ap.ssid, WIFI_PORTAL_SSID, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(WIFI_PORTAL_SSID);
    strncpy((char *)wifi_config.ap.password, WIFI_PORTAL_PASS, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.channel = WIFI_PORTAL_CHANNEL;
    wifi_config.ap.max_connection = WIFI_PORTAL_MAX_CONN;
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
    wifi_config.ap.authmode = WIFI_AUTH_WPA3_PSK;
    wifi_config.ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
#else
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
#endif
    wifi_config.ap.pmf_cfg.required = true;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        WP_ERROR("set mode failed: %s", esp_err_to_name(err));
        wifi_portal_teardown();
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        WP_ERROR("set config failed: %s", esp_err_to_name(err));
        wifi_portal_teardown();
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        WP_ERROR("Wi-Fi start failed: %s", esp_err_to_name(err));
        WP_ERROR("The ESP32-C6 must run ESP-Hosted SDIO slave firmware that matches this build");
        wifi_portal_teardown();
        return err;
    }

    s_started = true;
    WP_INFO("Hotspot %s up at %s", WIFI_PORTAL_SSID, WIFI_PORTAL_URL);
    return ESP_OK;
}
