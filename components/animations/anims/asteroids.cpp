#include <cmath>
#include <cstdlib>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Self-playing Asteroids: the ship sits in the center, turns toward the
// nearest rock with a limited turn rate, and fires when roughly aimed.
// Big asteroids split into two fragments; fragments burst into particles.
// Runs on a fixed 33 ms simulation tick from virtual time so the speed
// slider works.
class Asteroids final : public Animation {
public:
    const char* name() const override { return "Asteroids"; }

    void reset(panel::Matrix&) override {
        for (auto& r : roids_) r.alive = false;
        for (auto& b : bullets_) b.alive = false;
        for (auto& p : parts_) p.life = 0;
        angle_ = 0;
        cooldown_ = 0;
        respite_ = 30;
        wave_ = 0;
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        if (t_ms < last_ || t_ms - last_ > 500) last_ = t_ms;
        int guard = 0;
        while (t_ms - last_ >= 33 && ++guard <= 5) {
            sim();
            last_ += 33;
        }
        draw(m);
    }

private:
    static constexpr float kCx = 15.5f, kCy = 15.5f;
    static constexpr int kMaxRoids = 10;

    struct Roid {
        float x, y, vx, vy;
        int size;  // 2 = big, 1 = fragment
        bool alive;
    };
    struct Bullet {
        float x, y, vx, vy;
        bool alive;
    };
    struct Part {
        float x, y, vx, vy;
        int life;
        uint8_t hue;
    };

    static float frand(float lo, float hi) {
        return lo + (hi - lo) * (esp_random() % 1000) / 1000.0f;
    }

    static void line(panel::Matrix& m, int x0, int y0, int x1, int y1, Rgb c) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            m.set(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void spawn_wave() {
        wave_++;
        int n = 2 + (wave_ > 3 ? 3 : wave_);
        for (auto& r : roids_) {
            if (n == 0) break;
            if (r.alive) continue;
            switch (esp_random() % 4) {
                case 0: r.x = frand(0, 31); r.y = -2; break;
                case 1: r.x = frand(0, 31); r.y = 33; break;
                case 2: r.x = -2; r.y = frand(0, 31); break;
                default: r.x = 33; r.y = frand(0, 31); break;
            }
            float ang = atan2f(kCy - r.y + frand(-6, 6), kCx - r.x + frand(-6, 6));
            float sp = frand(0.08f, 0.16f);
            r.vx = cosf(ang) * sp;
            r.vy = sinf(ang) * sp;
            r.size = 2;
            r.alive = true;
            n--;
        }
    }

    void boom(float x, float y, int n) {
        for (auto& p : parts_) {
            if (n == 0) break;
            if (p.life > 0) continue;
            float ang = frand(0, 6.28f);
            float sp = frand(0.05f, 0.45f);
            p.x = x;
            p.y = y;
            p.vx = cosf(ang) * sp;
            p.vy = sinf(ang) * sp;
            p.life = 10 + esp_random() % 14;
            p.hue = 10 + esp_random() % 40;
            n--;
        }
    }

    void split(Roid& r) {
        boom(r.x, r.y, 8);
        r.size = 1;
        float ang = frand(0, 6.28f);
        float sp = frand(0.12f, 0.20f);
        r.vx = cosf(ang) * sp;
        r.vy = sinf(ang) * sp;
        for (auto& r2 : roids_) {
            if (r2.alive) continue;
            r2 = r;
            r2.vx = -r.vx + frand(-0.05f, 0.05f);
            r2.vy = -r.vy + frand(-0.05f, 0.05f);
            break;
        }
    }

    void sim() {
        // Aim at the nearest live asteroid.
        int target = -1;
        float best = 1e9f;
        for (int i = 0; i < kMaxRoids; i++) {
            if (!roids_[i].alive) continue;
            float dx = roids_[i].x - kCx, dy = roids_[i].y - kCy;
            float d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                target = i;
            }
        }
        if (target < 0) {
            // Field cleared: short breather, then the next wave rolls in.
            if (--respite_ <= 0) {
                spawn_wave();
                respite_ = 45;
            }
        } else {
            float want = atan2f(roids_[target].y - kCy, roids_[target].x - kCx);
            float diff = want - angle_;
            while (diff > (float)M_PI) diff -= 2 * (float)M_PI;
            while (diff < -(float)M_PI) diff += 2 * (float)M_PI;
            float turn = diff;
            if (turn > 0.12f) turn = 0.12f;
            if (turn < -0.12f) turn = -0.12f;
            angle_ += turn;

            if (cooldown_ > 0) cooldown_--;
            if (fabsf(diff) < 0.15f && cooldown_ == 0) {
                for (auto& b : bullets_) {
                    if (b.alive) continue;
                    b.alive = true;
                    b.x = kCx + cosf(angle_) * 3.6f;
                    b.y = kCy + sinf(angle_) * 3.6f;
                    b.vx = cosf(angle_) * 1.1f;
                    b.vy = sinf(angle_) * 1.1f;
                    cooldown_ = 10;
                    break;
                }
            }
        }

        for (auto& b : bullets_) {
            if (!b.alive) continue;
            b.x += b.vx;
            b.y += b.vy;
            if (b.x < -1 || b.x > 32 || b.y < -1 || b.y > 32) b.alive = false;
        }

        for (auto& r : roids_) {
            if (!r.alive) continue;
            r.x += r.vx;
            r.y += r.vy;
            float dx = r.x - kCx, dy = r.y - kCy;
            if (dx * dx + dy * dy < 2.0f) {
                // Reached the ship: dramatic burst (the demo ship survives).
                boom(r.x, r.y, 26);
                r.alive = false;
                continue;
            }
            if (r.x < -3 || r.x > 34 || r.y < -3 || r.y > 34) {
                // Drifted past everything: turn it back toward the middle.
                float ang = atan2f(kCy - r.y, kCx - r.x);
                float sp = sqrtf(r.vx * r.vx + r.vy * r.vy);
                r.vx = cosf(ang) * sp;
                r.vy = sinf(ang) * sp;
            }
        }

        for (auto& b : bullets_) {
            if (!b.alive) continue;
            for (auto& r : roids_) {
                if (!r.alive) continue;
                float dx = b.x - r.x, dy = b.y - r.y;
                float rad = r.size == 2 ? 1.6f : 0.9f;
                if (dx * dx + dy * dy < rad * rad) {
                    b.alive = false;
                    if (r.size == 2) {
                        split(r);
                    } else {
                        boom(r.x, r.y, 12);
                        r.alive = false;
                    }
                    break;
                }
            }
        }

        for (auto& p : parts_) {
            if (p.life <= 0) continue;
            p.x += p.vx;
            p.y += p.vy;
            p.life--;
        }
    }

    void draw(panel::Matrix& m) {
        m.fade(110);

        for (auto& p : parts_) {
            if (p.life <= 0) continue;
            int v = p.life * 18;
            m.add((int)p.x, (int)p.y, Rgb::hsv(p.hue, 230, v > 255 ? 255 : v));
        }
        for (auto& r : roids_) {
            if (!r.alive) continue;
            int x = (int)r.x, y = (int)r.y;
            Rgb c = Rgb::hsv(24, 140, 200);
            m.set(x, y, c);
            if (r.size == 2) {
                m.set(x + 1, y, c);
                m.set(x - 1, y, c);
                m.set(x, y + 1, c);
                m.set(x, y - 1, c);
            }
        }
        for (auto& b : bullets_) {
            if (b.alive) m.set((int)b.x, (int)b.y, {255, 255, 120});
        }

        // Classic wireframe triangle: nose ahead of center, two rear wings
        // swept back ~146 degrees. Drawn with lines so the hull stays
        // connected at every rotation angle.
        int nx = (int)lroundf(kCx + cosf(angle_) * 2.8f);
        int ny = (int)lroundf(kCy + sinf(angle_) * 2.8f);
        int lx = (int)lroundf(kCx + cosf(angle_ + 2.55f) * 2.3f);
        int ly = (int)lroundf(kCy + sinf(angle_ + 2.55f) * 2.3f);
        int rx = (int)lroundf(kCx + cosf(angle_ - 2.55f) * 2.3f);
        int ry = (int)lroundf(kCy + sinf(angle_ - 2.55f) * 2.3f);
        line(m, lx, ly, rx, ry, {90, 90, 120});
        line(m, nx, ny, lx, ly, {190, 190, 215});
        line(m, nx, ny, rx, ry, {190, 190, 215});
        m.set(nx, ny, {0, 255, 255});
    }

    Roid roids_[kMaxRoids];
    Bullet bullets_[6];
    Part parts_[40];
    float angle_ = 0;
    int cooldown_ = 0;
    int respite_ = 30;
    int wave_ = 0;
    uint32_t last_ = 0;
};

Animation* make_asteroids() {
    static Asteroids a;
    return &a;
}

}  // namespace anim
