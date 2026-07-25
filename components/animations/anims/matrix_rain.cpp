#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// "Matrix" digital rain: bright green heads falling at random speeds, the
// trails are left behind by fading the framebuffer between frames.
class MatrixRain final : public Animation {
public:
    const char* name() const override { return "Matrix Rain"; }

    void reset(panel::Matrix&) override {
        for (auto& d : drops_) respawn(d, true);
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        uint32_t dt = t_ms - last_;
        if (t_ms < last_) dt = 16;
        last_ = t_ms;

        m.fade(46);
        for (auto& d : drops_) {
            d.y += d.speed * dt;
            int y = d.y >> 8;
            if (y >= kHeight + 4) {
                respawn(d, false);
                continue;
            }
            m.set(d.x, y, {180, 255, 180});
            m.add(d.x, y - 1, {0, 140, 0});
        }
    }

private:
    struct Drop {
        int x;
        int32_t y;      // 24.8 fixed point
        int32_t speed;  // px per ms, 24.8 fixed point
    };

    void respawn(Drop& d, bool anywhere) {
        d.x = esp_random() % kWidth;
        d.y = anywhere ? (esp_random() % (kHeight << 8)) : -(2 << 8);
        d.speed = 3 + esp_random() % 6;  // 0.012..0.035 px/ms
    }

    Drop drops_[26];
    uint32_t last_ = 0;
};

Animation* make_matrix_rain() {
    static MatrixRain a;
    return &a;
}

}  // namespace anim
