#include <cmath>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Little physics sandbox: colored balls of different sizes drop in, bounce
// off the walls and floor, and collide elastically with each other
// (mass proportional to area). A ball that has settled on the floor is
// quietly relaunched from above so the sim never goes still.
class BouncingBalls final : public Animation {
public:
    const char* name() const override { return "Bouncing Balls"; }

    void reset(panel::Matrix&) override {
        for (int i = 0; i < kBalls; i++) {
            spawn(balls_[i]);
            // Stagger the initial drop heights so they don't arrive as one clump.
            balls_[i].y -= i * 7.0f;
        }
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        uint32_t dt = t_ms - last_;
        if (dt > 100) dt = 33;
        last_ = t_ms;
        float h = dt / 1000.0f / kSubsteps;

        for (int s = 0; s < kSubsteps; s++) {
            for (auto& b : balls_) {
                b.vy += kGravity * h;
                b.x += b.vx * h;
                b.y += b.vy * h;

                if (b.x < b.r) { b.x = b.r; b.vx = -b.vx * kWallRest; }
                if (b.x > kWidth - 1 - b.r) { b.x = kWidth - 1 - b.r; b.vx = -b.vx * kWallRest; }
                if (b.y > kHeight - 1 - b.r) {
                    b.y = kHeight - 1 - b.r;
                    b.vy = -b.vy * kWallRest;
                    b.vx *= 0.995f;  // rolling friction
                }
                // No ceiling: relaunched balls fall in from above.
            }
            collide();
        }

        // Relaunch balls that have settled on the floor.
        for (auto& b : balls_) {
            bool resting = b.y > kHeight - 1.6f - b.r &&
                           fabsf(b.vy) < 2.0f && fabsf(b.vx) < 2.0f;
            b.still_ms = resting ? b.still_ms + dt : 0;
            if (b.still_ms > 1600) spawn(b);
        }

        m.clear();
        for (const auto& b : balls_) {
            int x0 = static_cast<int>(b.x - b.r), x1 = static_cast<int>(b.x + b.r) + 1;
            int y0 = static_cast<int>(b.y - b.r), y1 = static_cast<int>(b.y + b.r) + 1;
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    float dx = x - b.x, dy = y - b.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 > b.r * b.r) continue;
                    uint8_t v = d2 > (b.r - 0.8f) * (b.r - 0.8f) ? 140 : 255;  // soft edge
                    m.add(x, y, Rgb::hsv(b.hue, 240, v));
                }
            }
        }
    }

private:
    struct Ball {
        float x, y, vx, vy, r, mass;
        uint8_t hue;
        uint32_t still_ms;
    };

    void spawn(Ball& b) {
        b.r = 1.3f + (esp_random() % 14) / 10.0f;
        b.mass = b.r * b.r;
        b.x = b.r + esp_random() % static_cast<int>(kWidth - 2 * b.r);
        b.y = -2.0f - esp_random() % 6;
        b.vx = (static_cast<int>(esp_random() % 25) - 12) * 0.7f;
        b.vy = 0.0f;
        b.hue = esp_random() % 256;
        b.still_ms = 0;
    }

    void collide() {
        for (int i = 0; i < kBalls; i++) {
            for (int j = i + 1; j < kBalls; j++) {
                Ball &a = balls_[i], &b = balls_[j];
                float dx = b.x - a.x, dy = b.y - a.y;
                float d2 = dx * dx + dy * dy;
                float min_d = a.r + b.r;
                if (d2 >= min_d * min_d || d2 < 1e-4f) continue;

                float d = sqrtf(d2);
                float nx = dx / d, ny = dy / d;

                // Separate the overlap, the lighter ball giving way more.
                float overlap = min_d - d;
                float total = a.mass + b.mass;
                a.x -= nx * overlap * (b.mass / total);
                a.y -= ny * overlap * (b.mass / total);
                b.x += nx * overlap * (a.mass / total);
                b.y += ny * overlap * (a.mass / total);

                // Elastic impulse along the collision normal.
                float approach = (a.vx - b.vx) * nx + (a.vy - b.vy) * ny;
                if (approach <= 0.0f) continue;  // already separating
                float p = (1.0f + kBallRest) * approach / total;
                a.vx -= p * b.mass * nx;
                a.vy -= p * b.mass * ny;
                b.vx += p * a.mass * nx;
                b.vy += p * a.mass * ny;
            }
        }
    }

    static constexpr int kBalls = 7;
    static constexpr int kSubsteps = 3;
    static constexpr float kGravity = 42.0f;   // px/s^2
    static constexpr float kWallRest = 0.86f;  // restitution vs walls/floor
    static constexpr float kBallRest = 0.92f;  // restitution ball vs ball

    Ball balls_[kBalls] = {};
    uint32_t last_ = 0;
};

Animation* make_bouncing_balls() {
    static BouncingBalls a;
    return &a;
}

}  // namespace anim
