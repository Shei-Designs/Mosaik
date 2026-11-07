#include "webctl/wifi.hpp"

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "nvs.h"
#include "sdkconfig.h"

namespace webctl {

static const char* TAG = "wifi";

static EventGroupHandle_t s_events;
static constexpr int kConnectedBit = BIT0;
static constexpr int kFailedBit = BIT1;
static int s_retries = 0;
static constexpr int kMaxRetries = 8;

static char s_ssid[33];
static char s_pass[65];
static char s_ip[16] = "0.0.0.0";
static bool s_ap_mode = false;

// Credentials saved from the web UI (NVS) take priority over Kconfig.
static void load_credentials() {
    std::strncpy(s_ssid, CONFIG_PANEL_WIFI_SSID, sizeof(s_ssid) - 1);
    std::strncpy(s_pass, CONFIG_PANEL_WIFI_PASSWORD, sizeof(s_pass) - 1);

    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return;
    char ssid[33] = {}, pass[65] = {};
    size_t sl = sizeof(ssid);
    if (nvs_get_str(h, "ssid", ssid, &sl) == ESP_OK && ssid[0]) {
        std::memset(s_ssid, 0, sizeof(s_ssid));
        std::memset(s_pass, 0, sizeof(s_pass));
        std::strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
        size_t pl = sizeof(pass);
        if (nvs_get_str(h, "pass", pass, &pl) == ESP_OK) {
            std::strncpy(s_pass, pass, sizeof(s_pass) - 1);
        }
    }
    nvs_close(h);
}

static void event_handler(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retries < kMaxRetries) {
            s_retries++;
            ESP_LOGW(TAG, "disconnected, retry %d/%d", s_retries, kMaxRetries);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_events, kFailedBit);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);
        std::snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got IP: %s", s_ip);
        s_retries = 0;
        xEventGroupSetBits(s_events, kConnectedBit);
    }
}

static void start_ap() {
    ESP_LOGW(TAG, "starting fallback AP \"%s\" (http://192.168.4.1)", CONFIG_PANEL_AP_SSID);
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t cfg = {};
    std::strncpy(reinterpret_cast<char*>(cfg.ap.ssid), CONFIG_PANEL_AP_SSID, sizeof(cfg.ap.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(cfg.ap.password), CONFIG_PANEL_AP_PASSWORD, sizeof(cfg.ap.password) - 1);
    cfg.ap.ssid_len = std::strlen(CONFIG_PANEL_AP_SSID);
    cfg.ap.channel = 1;
    cfg.ap.max_connection = 4;
    cfg.ap.authmode = std::strlen(CONFIG_PANEL_AP_PASSWORD) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_ap_mode = true;
    std::strcpy(s_ip, "192.168.4.1");
}

void wifi_start() {
    s_events = xEventGroupCreate();
    load_credentials();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr, nullptr));

    if (std::strlen(s_ssid) > 0) {
        wifi_config_t cfg = {};
        std::strncpy(reinterpret_cast<char*>(cfg.sta.ssid), s_ssid, sizeof(cfg.sta.ssid) - 1);
        std::strncpy(reinterpret_cast<char*>(cfg.sta.password), s_pass, sizeof(cfg.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(s_events, kConnectedBit | kFailedBit,
                                               pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
        if (!(bits & kConnectedBit)) {
            start_ap();
        }
    } else {
        ESP_LOGW(TAG, "no WiFi credentials configured");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        start_ap();
    }

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set(CONFIG_PANEL_HOSTNAME);
    mdns_instance_name_set("Mosaik");
    mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
    ESP_LOGI(TAG, "reachable at http://%s.local", CONFIG_PANEL_HOSTNAME);
}

bool wifi_in_ap_mode() { return s_ap_mode; }
const char* wifi_current_ssid() { return s_ssid; }
const char* wifi_ip() { return s_ip; }

static void restart_cb(void*) { esp_restart(); }

void wifi_save_credentials_and_restart(const char* ssid, const char* pass) {
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &h));
    if (ssid && ssid[0]) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "pass", pass ? pass : "");
    } else {
        // Empty SSID clears the saved credentials (back to Kconfig / AP).
        nvs_erase_all(h);
    }
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "credentials saved, restarting in 1.5 s");

    // Delay the restart so the HTTP response makes it out first.
    const esp_timer_create_args_t args = {
        .callback = &restart_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "restart",
        .skip_unhandled_events = false,
    };
    static esp_timer_handle_t timer = nullptr;
    if (!timer) {
        ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
        ESP_ERROR_CHECK(esp_timer_start_once(timer, 1500 * 1000));
    }
}

}  // namespace webctl
