#include <cmath>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// 3D starfield flying toward the viewer, perspective-projected onto the
// panel. Stars brighten as they approach; motion trails come from fading
// the previous frame instead of clearing it.
class Starfield final : public Animation {
public:
    const char* name() const override { return "Starfield"; }

    void reset(panel::Matrix&) override {
        for (auto& s : stars_) respawn(s, true);
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        uint32_t dt = t_ms - last_;
        if (t_ms < last_) dt = 16;
        last_ = t_ms;

        m.fade(70);
        for (auto& s : stars_) {
            s.z -= dt * 0.012f;
            if (s.z <= 0.5f) {
                respawn(s, false);
                continue;
            }
            int px = static_cast<int>(16.0f + s.x * 12.0f / s.z);
            int py = static_cast<int>(16.0f + s.y * 12.0f / s.z);
            if (px < 0 || px >= kWidth || py < 0 || py >= kHeight) {
                respawn(s, false);
                continue;
            }
            float depth = 1.0f - s.z / 32.0f;
            uint8_t v = static_cast<uint8_t>(40 + depth * 215);
            m.set(px, py, Rgb::hsv(s.hue, 40, v));
        }
    }

private:
    struct Star {
        float x, y, z;
        uint8_t hue;
    };

    void respawn(Star& s, bool anywhere) {
        s.x = static_cast<int>(esp_random() % 64) - 32.0f;
        s.y = static_cast<int>(esp_random() % 64) - 32.0f;
        s.z = anywhere ? 1.0f + (esp_random() % 310) / 10.0f : 32.0f;
        s.hue = 140 + esp_random() % 60;
    }

    Star stars_[48];
    uint32_t last_ = 0;
};

Animation* make_starfield() {
    static Starfield a;
    return &a;
}

}  // namespace anim
