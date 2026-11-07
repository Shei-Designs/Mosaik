#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "panel/driver.hpp"
#include "animations/engine.hpp"
#include "webctl/wifi.hpp"
#include "webctl/http.hpp"

static const char* TAG = "main";

extern "C" void app_main() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    panel::driver_init();
    anim::engine_start();
    webctl::wifi_start();
    webctl::http_start();

    // Everything came up, so an OTA image that reached this point is good:
    // confirm it so the bootloader stops considering a rollback.
    esp_ota_mark_app_valid_cancel_rollback();

    ESP_LOGI(TAG, "Mosaik up: %d animations registered", anim::engine_animation_count());
}
