#include <cstring>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Classic heat-diffusion fire: sparks are injected at the bottom row, heat
// rises with cooling and slight sideways spread, and the heat value maps
// through a black->red->yellow->white palette.
class Fire final : public Animation {
public:
    const char* name() const override { return "Fire"; }

    void reset(panel::Matrix&) override {
        std::memset(heat_, 0, sizeof(heat_));
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        if (t_ms - last_ >= 33 || t_ms < last_) {
            last_ = t_ms;
            simulate();
        }
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                m.set(x, y, heat_color(heat_[y][x]));
            }
        }
    }

private:
    void simulate() {
        // Cool every cell a little.
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                int cool = esp_random() % 14;
                heat_[y][x] = heat_[y][x] > cool ? heat_[y][x] - cool : 0;
            }
        }
        // Heat drifts upward, averaging the cells below.
        for (int y = 0; y < kHeight - 1; y++) {
            for (int x = 0; x < kWidth; x++) {
                int xl = x > 0 ? x - 1 : x;
                int xr = x < kWidth - 1 ? x + 1 : x;
                heat_[y][x] = (heat_[y + 1][xl] + 2 * heat_[y + 1][x] + heat_[y + 1][xr]) / 4;
            }
        }
        // Random sparks at the bottom.
        for (int x = 0; x < kWidth; x++) {
            if ((esp_random() & 0xFF) < 90) {
                int v = 160 + esp_random() % 96;
                heat_[kHeight - 1][x] = v > 255 ? 255 : v;
            }
        }
    }

    static Rgb heat_color(uint8_t h) {
        uint8_t t192 = (h * 192) >> 8;
        uint8_t ramp = (t192 & 63) << 2;
        if (t192 >= 128) return {255, 255, ramp};
        if (t192 >= 64) return {255, ramp, 0};
        return {ramp, 0, 0};
    }

    uint8_t heat_[kHeight][kWidth] = {};
    uint32_t last_ = 0;
};

Animation* make_fire() {
    static Fire a;
    return &a;
}

}  // namespace anim
