#include <cmath>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Craig Reynolds' boids (separation, alignment, cohesion) plus a predator:
// the red pixel constantly seeks the flock's center of mass, and boids have
// an exaggerated, inverse-distance flee response with a panic speed burst.
// The predator cruises slightly slower than a panicked boid, so it plows
// through the school and the flock splits and regroups around it — the
// tuna-through-sardines effect. Everything lives on a torus.
class Boids final : public Animation {
public:
    const char* name() const override { return "Boids"; }

    void reset(panel::Matrix&) override {
        for (int i = 0; i < kCount; i++) {
            Boid& b = boids_[i];
            b.x = esp_random() % kWidth;
            b.y = esp_random() % kHeight;
            float ang = (esp_random() % 628) / 100.0f;
            b.vx = cosf(ang);
            b.vy = sinf(ang);
            b.hue = 130 + i * 6;
        }
        pred_x_ = 0;
        pred_y_ = 0;
        pred_vx_ = 0.5f;
        pred_vy_ = 0.3f;
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        float dt = (t_ms - last_) / 33.0f;
        if (t_ms < last_ || dt > 3.0f) dt = 1.0f;
        last_ = t_ms;

        update_predator(dt);
        for (int i = 0; i < kCount; i++) update(i, dt);

        m.fade(60);
        for (auto& b : boids_) {
            m.set(static_cast<int>(b.x), static_cast<int>(b.y), Rgb::hsv(b.hue, 200, 255));
        }
        m.set(static_cast<int>(pred_x_), static_cast<int>(pred_y_), {255, 24, 24});
    }

private:
    static constexpr int kCount = 14;
    static constexpr float kMaxSpeed = 0.9f;    // cruise
    static constexpr float kPanicSpeed = 1.35f; // fleeing burst
    static constexpr float kPredSpeed = 0.8f;   // predator top speed
    static constexpr float kFleeRadius = 10.0f;

    struct Boid {
        float x, y, vx, vy;
        uint8_t hue;
    };

    // Shortest wrapped delta from a to b on the torus.
    static float wrap_d(float a, float b, int size) {
        float d = b - a;
        if (d > size / 2.0f) d -= size;
        if (d < -size / 2.0f) d += size;
        return d;
    }

    static void wrap_pos(float& x, float& y) {
        if (x < 0) x += kWidth;
        if (x >= kWidth) x -= kWidth;
        if (y < 0) y += kHeight;
        if (y >= kHeight) y -= kHeight;
    }

    void update_predator(float dt) {
        // Steer toward the flock's center of mass. Averaging the wrapped
        // offsets from the predator's own position handles the torus.
        float mx = 0, my = 0;
        for (auto& b : boids_) {
            mx += wrap_d(pred_x_, b.x, kWidth);
            my += wrap_d(pred_y_, b.y, kHeight);
        }
        mx /= kCount;
        my /= kCount;
        float d = sqrtf(mx * mx + my * my) + 0.1f;
        pred_vx_ += mx / d * 0.05f * dt;
        pred_vy_ += my / d * 0.05f * dt;

        float speed = sqrtf(pred_vx_ * pred_vx_ + pred_vy_ * pred_vy_);
        if (speed > kPredSpeed) {
            pred_vx_ = pred_vx_ / speed * kPredSpeed;
            pred_vy_ = pred_vy_ / speed * kPredSpeed;
        } else if (speed < 0.35f && speed > 0.001f) {
            // Never let it stall in the middle of a scattered school.
            pred_vx_ = pred_vx_ / speed * 0.35f;
            pred_vy_ = pred_vy_ / speed * 0.35f;
        }
        pred_x_ += pred_vx_ * dt;
        pred_y_ += pred_vy_ * dt;
        wrap_pos(pred_x_, pred_y_);
    }

    void update(int i, float dt) {
        Boid& b = boids_[i];
        float cx = 0, cy = 0, ax = 0, ay = 0, sx = 0, sy = 0;
        int neighbors = 0;
        for (int j = 0; j < kCount; j++) {
            if (j == i) continue;
            float dx = wrap_d(b.x, boids_[j].x, kWidth);
            float dy = wrap_d(b.y, boids_[j].y, kHeight);
            float d2 = dx * dx + dy * dy;
            if (d2 > 64.0f) continue;  // neighbor radius 8
            neighbors++;
            cx += dx;
            cy += dy;
            ax += boids_[j].vx;
            ay += boids_[j].vy;
            if (d2 < 12.0f && d2 > 0.01f) {
                sx -= dx / d2;
                sy -= dy / d2;
            }
        }
        if (neighbors) {
            b.vx += (cx / neighbors) * 0.008f + (ax / neighbors - b.vx) * 0.05f + sx * 0.12f;
            b.vy += (cy / neighbors) * 0.008f + (ay / neighbors - b.vy) * 0.05f + sy * 0.12f;
        }

        // Exaggerated predator avoidance: force ramps up sharply as the
        // predator closes in, and fleeing boids get a panic speed burst.
        float pdx = wrap_d(pred_x_, b.x, kWidth);
        float pdy = wrap_d(pred_y_, b.y, kHeight);
        float pd = sqrtf(pdx * pdx + pdy * pdy) + 0.2f;
        bool fleeing = pd < kFleeRadius;
        if (fleeing) {
            float urgency = (kFleeRadius - pd) / kFleeRadius;  // 0..1
            b.vx += pdx / pd * urgency * 0.55f * dt;
            b.vy += pdy / pd * urgency * 0.55f * dt;
        }

        // Tiny random jitter keeps the flock from freezing into a line.
        b.vx += ((int)(esp_random() % 100) - 50) / 2500.0f;
        b.vy += ((int)(esp_random() % 100) - 50) / 2500.0f;

        float max_speed = fleeing ? kPanicSpeed : kMaxSpeed;
        float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
        if (speed > max_speed) {
            b.vx = b.vx / speed * max_speed;
            b.vy = b.vy / speed * max_speed;
        } else if (speed < 0.3f && speed > 0.001f) {
            b.vx = b.vx / speed * 0.3f;
            b.vy = b.vy / speed * 0.3f;
        }

        b.x += b.vx * dt;
        b.y += b.vy * dt;
        wrap_pos(b.x, b.y);
    }

    Boid boids_[kCount];
    float pred_x_ = 0, pred_y_ = 0, pred_vx_ = 0.5f, pred_vy_ = 0.3f;
    uint32_t last_ = 0;
};

Animation* make_boids() {
    static Boids a;
    return &a;
}

}  // namespace anim
