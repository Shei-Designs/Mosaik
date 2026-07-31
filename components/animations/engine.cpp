#include "animations/engine.hpp"

#include <atomic>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "panel/driver.hpp"

namespace anim {

static const char* TAG = "engine";

// ---------------------------------------------------------------------------
// Pixel Art: displays the last frame uploaded from the web UI.

static uint8_t s_art[panel::kNumLeds * 3];
static SemaphoreHandle_t s_art_mutex;

class PixelArt final : public Animation {
public:
    const char* name() const override { return "Pixel Art"; }
    void step(panel::Matrix& m, uint32_t) override {
        xSemaphoreTake(s_art_mutex, portMAX_DELAY);
        panel::Rgb* px = m.raw();
        for (int i = 0; i < panel::kNumLeds; i++) {
            px[i] = {s_art[i * 3], s_art[i * 3 + 1], s_art[i * 3 + 2]};
        }
        xSemaphoreGive(s_art_mutex);
    }
};

// ---------------------------------------------------------------------------
// Registry. Factories live in the individual animation files.

Animation* make_plasma();
Animation* make_game_of_life();
Animation* make_starfield();
Animation* make_fire();
Animation* make_sorting();
Animation* make_maze();
Animation* make_langton();
Animation* make_matrix_rain();
Animation* make_boids();
Animation* make_asteroids();
Animation* make_fireworks();
Animation* make_lava_lamp();
Animation* make_snake();
Animation* make_synthwave();
Animation* make_cube3d();
Animation* make_fractal_zoom();
Animation* make_bouncing_balls();

static Animation** registry() {
    static PixelArt art;
    static Animation* anims[] = {
        make_plasma(),
        make_game_of_life(),
        make_starfield(),
        make_fire(),
        make_sorting(),
        make_maze(),
        make_langton(),
        make_matrix_rain(),
        make_boids(),
        make_asteroids(),
        make_fireworks(),
        make_lava_lamp(),
        make_snake(),
        make_synthwave(),
        make_cube3d(),
        make_fractal_zoom(),
        make_bouncing_balls(),
        &art,
    };
    return anims;
}

static constexpr int kAnimCount = 18;
static constexpr int kPixelArtIndex = kAnimCount - 1;

// ---------------------------------------------------------------------------
// Engine state

static std::atomic<int> s_requested{0};
static std::atomic<uint8_t> s_brightness{160};
static std::atomic<uint8_t> s_speed{128};
static std::atomic<bool> s_art_dirty{false};

// NVS writes stall the flash cache and visibly glitch the render task, so
// setters only mark state dirty; the engine task persists once things have
// been quiet for a couple of seconds.
static std::atomic<bool> s_dirty{false};
static std::atomic<int64_t> s_changed_us{0};

static void mark_dirty() {
    s_changed_us.store(esp_timer_get_time());
    s_dirty.store(true);
}

static void save_state() {
    nvs_handle_t h;
    if (nvs_open("panel", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, "anim", s_requested.load());
    nvs_set_u8(h, "bright", s_brightness.load());
    nvs_set_u8(h, "speed", s_speed.load());
    if (s_art_dirty.exchange(false)) {
        xSemaphoreTake(s_art_mutex, portMAX_DELAY);
        nvs_set_blob(h, "frame", s_art, sizeof(s_art));
        xSemaphoreGive(s_art_mutex);
    }
    nvs_commit(h);
    nvs_close(h);
}

static void load_state() {
    nvs_handle_t h;
    if (nvs_open("panel", NVS_READONLY, &h) != ESP_OK) return;
    int32_t a = 0;
    uint8_t v = 0;
    if (nvs_get_i32(h, "anim", &a) == ESP_OK && a >= 0 && a < kAnimCount) {
        s_requested.store(a);
    }
    if (nvs_get_u8(h, "bright", &v) == ESP_OK) s_brightness.store(v);
    if (nvs_get_u8(h, "speed", &v) == ESP_OK) s_speed.store(v);
    size_t len = sizeof(s_art);
    nvs_get_blob(h, "frame", s_art, &len);
    nvs_close(h);
}

static void engine_task(void*) {
    Animation** anims = registry();
    panel::Matrix matrix;

    int current = -1;
    uint32_t vt_ms = 0;
    int64_t last_us = esp_timer_get_time();

    while (true) {
        int req = s_requested.load();
        if (req != current) {
            current = req;
            matrix.clear();
            anims[current]->reset(matrix);
            ESP_LOGI(TAG, "animation -> %s", anims[current]->name());
        }

        int64_t now_us = esp_timer_get_time();
        uint32_t dt_ms = (now_us - last_us) / 1000;
        last_us = now_us;
        if (dt_ms > 100) dt_ms = 100;
        vt_ms += (dt_ms * s_speed.load()) / 128;

        anims[current]->step(matrix, vt_ms);
        panel::driver_show(matrix, s_brightness.load());

        // Persist settings/art only after 2s of no changes; driver_show has
        // finished the strip refresh at this point, so the flash stall from
        // the NVS write can't tear a frame.
        if (s_dirty.load() && esp_timer_get_time() - s_changed_us.load() > 2000000) {
            s_dirty.store(false);
            save_state();
        }

        // driver_show blocks for the ~31 ms strip refresh; the delay just
        // yields and paces us when the frame was cheap.
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void engine_start() {
    s_art_mutex = xSemaphoreCreateMutex();
    load_state();
    xTaskCreatePinnedToCore(engine_task, "anim", 8192, nullptr, 5, nullptr, 1);
}

int engine_animation_count() { return kAnimCount; }

const char* engine_animation_name(int i) {
    if (i < 0 || i >= kAnimCount) return "";
    return registry()[i]->name();
}

void engine_set_animation(int i) {
    if (i < 0 || i >= kAnimCount) return;
    s_requested.store(i);
    mark_dirty();
}

void engine_set_brightness(uint8_t b) {
    s_brightness.store(b);
    mark_dirty();
}

void engine_set_speed(uint8_t s) {
    if (s < 8) s = 8;
    s_speed.store(s);
    mark_dirty();
}

Status engine_status() {
    return Status{s_requested.load(), s_brightness.load(), s_speed.load()};
}

bool engine_show_frame(const uint8_t* rgb, size_t len) {
    if (len != sizeof(s_art)) return false;
    xSemaphoreTake(s_art_mutex, portMAX_DELAY);
    std::memcpy(s_art, rgb, sizeof(s_art));
    xSemaphoreGive(s_art_mutex);
    s_art_dirty.store(true);
    s_requested.store(kPixelArtIndex);
    mark_dirty();
    return true;
}

}  // namespace anim
