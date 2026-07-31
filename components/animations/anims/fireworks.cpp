#include <cmath>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Rockets climb from the bottom edge and burst at a random apex into a
// ring of gravity-affected sparks. Trails come from fading the previous
// frame instead of clearing it.
class Fireworks final : public Animation {
public:
    const char* name() const override { return "Fireworks"; }

    void reset(panel::Matrix&) override {
        for (auto& r : rockets_) r.alive = false;
        for (auto& s : sparks_) s.life = 0.0f;
        last_ = 0;
        next_launch_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        uint32_t dt = t_ms - last_;
        if (dt > 100) dt = 33;
        last_ = t_ms;
        float dts = dt / 1000.0f;

        m.fade(55);

        if (t_ms >= next_launch_) {
            launch();
            next_launch_ = t_ms + 350 + esp_random() % 1100;
        }

        for (auto& r : rockets_) {
            if (!r.alive) continue;
            r.x += r.vx * dts;
            r.y += r.vy * dts;
            r.vy += 12.0f * dts;  // gravity slows the climb
            if (r.y <= r.burst_y || r.vy > -3.0f) {
                burst(r);
                r.alive = false;
                continue;
            }
            m.add(static_cast<int>(r.x), static_cast<int>(r.y), Rgb{255, 210, 120});
        }

        for (auto& s : sparks_) {
            if (s.life <= 0.0f) continue;
            s.life -= dts;
            s.x += s.vx * dts;
            s.y += s.vy * dts;
            s.vy += 9.0f * dts;
            s.vx *= 1.0f - 0.8f * dts;  // air drag
            uint8_t v = s.life > 1.0f ? 255 : static_cast<uint8_t>(s.life * 255);
            // Sparks flare white at first, then settle into their hue.
            uint8_t sat = s.life > 1.1f ? 120 : 255;
            m.add(static_cast<int>(s.x), static_cast<int>(s.y), Rgb::hsv(s.hue, sat, v));
        }
    }

private:
    struct Rocket {
        float x, y, vx, vy, burst_y;
        bool alive;
    };
    struct Spark {
        float x, y, vx, vy, life;
        uint8_t hue;
    };

    void launch() {
        for (auto& r : rockets_) {
            if (r.alive) continue;
            r.x = 6.0f + esp_random() % 20;
            r.y = kHeight - 1.0f;
            r.vx = (static_cast<int>(esp_random() % 9) - 4) * 0.4f;
            r.vy = -(16.0f + esp_random() % 10);
            r.burst_y = 3.0f + esp_random() % 10;
            r.alive = true;
            return;
        }
    }

    void burst(const Rocket& r) {
        uint8_t hue = esp_random() % 256;
        int n = 18 + esp_random() % 12;
        for (auto& s : sparks_) {
            if (s.life > 0.0f) continue;
            float a = (esp_random() % 628) / 100.0f;
            float sp = 3.0f + (esp_random() % 90) / 10.0f;
            s.x = r.x;
            s.y = r.y;
            s.vx = cosf(a) * sp;
            s.vy = sinf(a) * sp;
            s.life = 0.9f + (esp_random() % 60) / 100.0f;
            // Mostly the burst's color, a few off-hue strays for sparkle.
            s.hue = hue + (esp_random() % 8 ? 0 : esp_random() % 40);
            if (--n == 0) return;
        }
    }

    Rocket rockets_[4] = {};
    Spark sparks_[120] = {};
    uint32_t last_ = 0;
    uint32_t next_launch_ = 0;
};

Animation* make_fireworks() {
    static Fireworks a;
    return &a;
}

}  // namespace anim
