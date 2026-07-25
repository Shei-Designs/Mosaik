#include "panel/driver.hpp"

#include <atomic>
#include <cmath>

#include "esp_check.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

namespace panel {

static const char* TAG = "panel";

static led_strip_handle_t s_strip = nullptr;
static uint8_t s_gamma[256];
static std::atomic<uint32_t> s_last_ma{0};

// WS2812B draws roughly 20 mA per fully-lit color channel plus ~1 mA idle.
static constexpr uint32_t kIdleUaPerLed = 1000;
static constexpr uint32_t kChannelUaFull = 20000;

void driver_init() {
    // Gamma 2.2 lookup, keeps very low values from vanishing entirely.
    for (int i = 0; i < 256; i++) {
        float lin = i / 255.0f;
        int v = static_cast<int>(powf(lin, 2.2f) * 255.0f + 0.5f);
        if (i > 0 && v == 0) v = 1;
        s_gamma[i] = static_cast<uint8_t>(v);
    }

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = CONFIG_PANEL_DATA_GPIO;
    strip_config.max_leds = kNumLeds;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out = false;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    // DMA offloads the whole 1024-LED frame so WiFi interrupt latency can't
    // glitch the strip timing.
    rmt_config.mem_block_symbols = 1024;
    rmt_config.flags.with_dma = true;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));
    ESP_ERROR_CHECK(led_strip_clear(s_strip));
    ESP_LOGI(TAG, "led_strip ready: %d LEDs on GPIO %d", kNumLeds, CONFIG_PANEL_DATA_GPIO);
}

void driver_show(const Matrix& m, uint8_t brightness) {
    if (!s_strip) return;

    static uint8_t out[kNumLeds][3];

    uint64_t total_ua = static_cast<uint64_t>(kNumLeds) * kIdleUaPerLed;
    const Rgb* px = m.raw();
    for (int y = 0; y < kHeight; y++) {
        for (int x = 0; x < kWidth; x++) {
            const Rgb& c = px[y * kWidth + x];
            uint16_t idx = xy_to_strip_index(x, y);
            uint8_t r = s_gamma[(c.r * brightness) >> 8];
            uint8_t g = s_gamma[(c.g * brightness) >> 8];
            uint8_t b = s_gamma[(c.b * brightness) >> 8];
            out[idx][0] = r;
            out[idx][1] = g;
            out[idx][2] = b;
            total_ua += (r + g + b) * (kChannelUaFull / 255);
        }
    }

    // Scale the whole frame down if it would exceed the configured budget.
    uint32_t total_ma = total_ua / 1000;
    uint32_t budget_ma = CONFIG_PANEL_MAX_CURRENT_MA;
    uint32_t scale = 256;
    if (total_ma > budget_ma) {
        scale = (budget_ma * 256) / total_ma;
        if (scale < 1) scale = 1;
    }

    for (int i = 0; i < kNumLeds; i++) {
        uint8_t r = (out[i][0] * scale) >> 8;
        uint8_t g = (out[i][1] * scale) >> 8;
        uint8_t b = (out[i][2] * scale) >> 8;
        led_strip_set_pixel(s_strip, i, r, g, b);
    }

    s_last_ma.store(total_ma > budget_ma ? budget_ma : total_ma);
    led_strip_refresh(s_strip);
}

uint32_t driver_last_frame_ma() {
    return s_last_ma.load();
}

}  // namespace panel
