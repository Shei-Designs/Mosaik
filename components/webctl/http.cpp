#include "webctl/http.hpp"

#include <cstdlib>
#include <cstring>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"

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
extern const char icon_svg_start[] asm("_binary_icon_svg_start");
extern const char icon_svg_end[] asm("_binary_icon_svg_end");
extern const char icon_180_png_start[] asm("_binary_icon_180_png_start");
extern const char icon_180_png_end[] asm("_binary_icon_180_png_end");
extern const char inter_400_woff2_start[] asm("_binary_inter_400_woff2_start");
extern const char inter_400_woff2_end[] asm("_binary_inter_400_woff2_end");
extern const char inter_600_woff2_start[] asm("_binary_inter_600_woff2_start");
extern const char inter_600_woff2_end[] asm("_binary_inter_600_woff2_end");
extern const char space_grotesk_500_woff2_start[] asm("_binary_space_grotesk_500_woff2_start");
extern const char space_grotesk_500_woff2_end[] asm("_binary_space_grotesk_500_woff2_end");
extern const char jetbrains_mono_400_woff2_start[] asm("_binary_jetbrains_mono_400_woff2_start");
extern const char jetbrains_mono_400_woff2_end[] asm("_binary_jetbrains_mono_400_woff2_end");
}

static esp_err_t root_get(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

// Static files embedded in flash: the brand fonts (served locally, since in
// access-point mode the phone has no internet) and the Mosaik tile icon.
// They only change with a firmware update, which reloads the page anyway,
// so browsers may cache them for a day.
struct Asset {
    const char* uri;
    const char* type;
    const char* start;
    const char* end;
};

static const Asset kAssets[] = {
    {"/icon.svg", "image/svg+xml", icon_svg_start, icon_svg_end},
    {"/icon-180.png", "image/png", icon_180_png_start, icon_180_png_end},
    {"/fonts/inter-400.woff2", "font/woff2", inter_400_woff2_start, inter_400_woff2_end},
    {"/fonts/inter-600.woff2", "font/woff2", inter_600_woff2_start, inter_600_woff2_end},
    {"/fonts/space-grotesk-500.woff2", "font/woff2", space_grotesk_500_woff2_start, space_grotesk_500_woff2_end},
    {"/fonts/jetbrains-mono-400.woff2", "font/woff2", jetbrains_mono_400_woff2_start, jetbrains_mono_400_woff2_end},
};

static esp_err_t asset_get(httpd_req_t* req) {
    const Asset* a = static_cast<const Asset*>(req->user_ctx);
    httpd_resp_set_type(req, a->type);
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    return httpd_resp_send(req, a->start, a->end - a->start);
}

static esp_err_t status_get(httpd_req_t* req) {
    anim::Status st = anim::engine_status();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "anim", st.anim);
    cJSON_AddNumberToObject(root, "brightness", st.brightness);
    cJSON_AddNumberToObject(root, "speed", st.speed);
    cJSON_AddNumberToObject(root, "ma", panel::driver_last_frame_ma());
    cJSON_AddStringToObject(root, "version", esp_app_get_description()->version);
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

static void restart_cb(void*) { esp_restart(); }

static void restart_soon() {
    // Delay the restart so the HTTP response makes it out first.
    const esp_timer_create_args_t args = {
        .callback = &restart_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_restart",
        .skip_unhandled_events = false,
    };
    static esp_timer_handle_t timer = nullptr;
    if (!timer) {
        ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
        ESP_ERROR_CHECK(esp_timer_start_once(timer, 1500 * 1000));
    }
}

// POST /api/ota  body: raw application image (build/mosaik.bin).
// Writes it to the inactive app slot, verifies it, marks it bootable and
// restarts. If the new image fails to boot, the bootloader rolls back.
static esp_err_t ota_post(httpd_req_t* req) {
    const esp_partition_t* dst = esp_ota_get_next_update_partition(nullptr);
    if (!dst) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no OTA partition");
    }
    if (req->content_len == 0 || req->content_len > dst->size) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad image size");
    }
    ESP_LOGI(TAG, "OTA: %u bytes -> %s", (unsigned)req->content_len, dst->label);

    esp_ota_handle_t ota;
    esp_err_t err = esp_ota_begin(dst, req->content_len, &ota);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "flash erase failed");
    }

    static uint8_t buf[4096];
    size_t remaining = req->content_len;
    while (remaining > 0) {
        int r = httpd_req_recv(req, reinterpret_cast<char*>(buf),
                               remaining < sizeof(buf) ? remaining : sizeof(buf));
        if (r <= 0) {
            esp_ota_abort(ota);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
        }
        if ((err = esp_ota_write(ota, buf, r)) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
            esp_ota_abort(ota);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "flash write failed");
        }
        remaining -= r;
    }

    if ((err = esp_ota_end(ota)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                   err == ESP_ERR_OTA_VALIDATE_FAILED ? "image validation failed"
                                                                      : "flash finalize failed");
    }
    if ((err = esp_ota_set_boot_partition(dst)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set boot failed");
    }

    ESP_LOGI(TAG, "OTA ok, restarting into %s", dst->label);
    err = httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");
    restart_soon();
    return err;
}

void http_start() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 16;

    httpd_handle_t server = nullptr;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get, .user_ctx = nullptr},
        {.uri = "/api/state", .method = HTTP_POST, .handler = state_post, .user_ctx = nullptr},
        {.uri = "/api/frame", .method = HTTP_POST, .handler = frame_post, .user_ctx = nullptr},
        {.uri = "/api/wifi", .method = HTTP_GET, .handler = wifi_get, .user_ctx = nullptr},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post, .user_ctx = nullptr},
        {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post, .user_ctx = nullptr},
    };
    for (auto& r : routes) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &r));
    }
    for (auto& a : kAssets) {
        httpd_uri_t r = {.uri = a.uri, .method = HTTP_GET, .handler = asset_get,
                         .user_ctx = const_cast<Asset*>(&a)};
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &r));
    }
    ESP_LOGI(TAG, "http server started");
}

}  // namespace webctl
