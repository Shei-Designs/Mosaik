#include "webctl/http.hpp"

#include <cstdlib>
#include <cstring>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "animations/engine.hpp"
#include "panel/driver.hpp"
#include "panel/matrix.hpp"
#include "sdkconfig.h"
#include "webctl/wifi.hpp"

namespace webctl {

static const char* TAG = "http";

extern "C" {
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");
}

static esp_err_t root_get(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t status_get(httpd_req_t* req) {
    anim::Status st = anim::engine_status();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "anim", st.anim);
    cJSON_AddNumberToObject(root, "brightness", st.brightness);
    cJSON_AddNumberToObject(root, "speed", st.speed);
    cJSON_AddNumberToObject(root, "ma", panel::driver_last_frame_ma());
    cJSON* list = cJSON_AddArrayToObject(root, "animations");
    for (int i = 0; i < anim::engine_animation_count(); i++) {
        cJSON_AddItemToArray(list, cJSON_CreateString(anim::engine_animation_name(i)));
    }

    char* out = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, out);
    cJSON_free(out);
    cJSON_Delete(root);
    return err;
}

// POST /api/state  body: {"anim": 2, "brightness": 200, "speed": 128}
// All fields optional.
static esp_err_t state_post(httpd_req_t* req) {
    char buf[192];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
    buf[len] = 0;

    cJSON* root = cJSON_Parse(buf);
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");

    cJSON* v;
    if ((v = cJSON_GetObjectItem(root, "anim")) && cJSON_IsNumber(v)) {
        anim::engine_set_animation(v->valueint);
    }
    if ((v = cJSON_GetObjectItem(root, "brightness")) && cJSON_IsNumber(v)) {
        int b = v->valueint;
        anim::engine_set_brightness(b < 0 ? 0 : b > 255 ? 255 : b);
    }
    if ((v = cJSON_GetObjectItem(root, "speed")) && cJSON_IsNumber(v)) {
        int s = v->valueint;
        anim::engine_set_speed(s < 0 ? 0 : s > 255 ? 255 : s);
    }
    cJSON_Delete(root);
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

// POST /api/frame  body: raw 3072 bytes, RGB row-major from top-left.
static esp_err_t frame_post(httpd_req_t* req) {
    constexpr size_t kFrameBytes = panel::kNumLeds * 3;
    if (req->content_len != kFrameBytes) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "expected 3072 bytes");
    }

    static uint8_t frame[kFrameBytes];
    size_t got = 0;
    while (got < kFrameBytes) {
        int r = httpd_req_recv(req, reinterpret_cast<char*>(frame) + got, kFrameBytes - got);
        if (r <= 0) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
        got += r;
    }

    anim::engine_show_frame(frame, kFrameBytes);
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

// GET /api/wifi -> current connection info (never returns the password).
static esp_err_t wifi_get(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mode", wifi_in_ap_mode() ? "ap" : "sta");
    cJSON_AddStringToObject(root, "ssid", wifi_current_ssid());
    cJSON_AddStringToObject(root, "ip", wifi_ip());
    cJSON_AddStringToObject(root, "hostname", CONFIG_PANEL_HOSTNAME);

    char* out = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, out);
    cJSON_free(out);
    cJSON_Delete(root);
    return err;
}

// POST /api/wifi  body: {"ssid": "...", "password": "..."}
// Saves the credentials and restarts the panel. Empty SSID clears them.
static esp_err_t wifi_post(httpd_req_t* req) {
    char buf[192];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
    buf[len] = 0;

    cJSON* root = cJSON_Parse(buf);
    if (!root) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");

    cJSON* jssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* jpass = cJSON_GetObjectItem(root, "password");
    const char* ssid = cJSON_IsString(jssid) ? jssid->valuestring : "";
    const char* pass = cJSON_IsString(jpass) ? jpass->valuestring : "";

    if (std::strlen(ssid) > 32 || std::strlen(pass) > 64 ||
        (ssid[0] && pass[0] && std::strlen(pass) < 8)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   "SSID max 32 chars; password 8-64 chars (or empty for an open network)");
    }

    esp_err_t err = httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");
    wifi_save_credentials_and_restart(ssid, pass);
    cJSON_Delete(root);
    return err;
}

void http_start() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get, .user_ctx = nullptr},
        {.uri = "/api/state", .method = HTTP_POST, .handler = state_post, .user_ctx = nullptr},
        {.uri = "/api/frame", .method = HTTP_POST, .handler = frame_post, .user_ctx = nullptr},
        {.uri = "/api/wifi", .method = HTTP_GET, .handler = wifi_get, .user_ctx = nullptr},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post, .user_ctx = nullptr},
    };
    for (auto& r : routes) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &r));
    }
    ESP_LOGI(TAG, "http server started");
}

}  // namespace webctl
