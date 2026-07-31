#include <cmath>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Driving into an 80s synthwave sunset. Composition, top to bottom:
// twinkling, slowly re-seeding stars in a violet sky, a smooth
// yellow-to-red gradient sun resting on a horizon whose shimmer radiates
// from the center outward, and a dark plain with a road converging on the
// vanishing point. Either side of the road is a field of pink dots that
// step down the screen in unison -- slower than the road's dashes, for
// parallax -- and veer outward near the bottom so they never touch the
// road; dots that leave the screen respawn at the horizon.
class Synthwave final : public Animation {
public:
    const char* name() const override { return "Synthwave"; }

    void reset(panel::Matrix&) override {
        for (int i = 0; i < kStars; i++) reseed_star(i);
        // Scatter the dot field over the whole plain to start with.
        for (auto& d : dots_) spawn_dot(d, /*at_top=*/false);
        for (auto& ds : dashes_) ds.active = false;
        next_shift_ = 0;
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        float t = t_ms / 1000.0f;
        uint32_t dt = t_ms - last_;
        if (dt > 100) dt = 33;
        last_ = t_ms;
        constexpr float kCx = 15.5f;

        // --- sky: deep violet fading warm toward the horizon ---
        for (int y = 0; y < kHorizon; y++) {
            float u = static_cast<float>(y) / kHorizon;
            Rgb sky{static_cast<uint8_t>(18 + u * 110),
                    static_cast<uint8_t>(u * u * 30),
                    static_cast<uint8_t>(44 + u * 36)};
            for (int x = 0; x < kWidth; x++) m.set(x, y, sky);
        }

        // --- stars: each twinkles at its own rate, and while fully dark
        // it may quietly move elsewhere so the field keeps changing ---
        for (int i = 0; i < kStars; i++) {
            float tw = sinf(t * star_rate_[i] + star_ph_[i] * 0.0246f);
            if (tw > 0.0f) {
                uint8_t v = static_cast<uint8_t>(60 + tw * 170);
                m.set(star_x_[i], star_y_[i], Rgb{v, v, static_cast<uint8_t>(v / 2 + 90)});
            } else if (tw < -0.9f && esp_random() % 6 == 0) {
                reseed_star(i);
            }
        }

        // --- sun: smooth gradient disc sitting on the horizon ---
        constexpr float kSunY = 11.0f, kSunR = 6.2f;
        for (int y = 5; y < kHorizon; y++) {
            for (int x = 9; x < 23; x++) {
                float dx = x - kCx, dy = y - kSunY;
                float d = sqrtf(dx * dx + dy * dy);
                if (d >= kSunR) continue;
                // Yellow at the crown to deep red-orange at the base.
                float sv = (y - (kSunY - kSunR)) / (2.0f * kSunR);
                uint8_t hue = static_cast<uint8_t>(42.0f - sv * 40.0f);
                uint8_t v = d > kSunR - 1.3f ? 190 : 255;  // soft rim
                m.set(x, y, Rgb::hsv(hue, 255, v));
            }
        }

        // --- horizon: shimmer radiating from the center out to both
        // edges. Under the sun the line takes on the sun's red-orange
        // base color, then blends around the wheel to hot pink once past
        // the sun's edge, with a slight glow boost at the center ---
        for (int x = 0; x < kWidth; x++) {
            float dx = x - kCx;
            if (dx < 0) dx = -dx;
            float u = (dx - 3.2f) / 4.0f;  // 0 under the sun -> 1 beyond its edge
            if (u < 0.0f) u = 0.0f;
            if (u > 1.0f) u = 1.0f;
            uint8_t hue = static_cast<uint8_t>(4.0f - u * 32.0f);  // wraps to 228
            uint8_t sat = static_cast<uint8_t>(255 - u * 45.0f);
            int v = static_cast<int>(195 + 60.0f * sinf(dx * 0.55f - t * 2.2f) + (1.0f - u) * 40.0f);
            m.set(x, kHorizon, Rgb::hsv(hue, sat, v > 255 ? 255 : static_cast<uint8_t>(v)));
        }

        // --- ground: dark plain and the road ---
        for (int y = kHorizon + 1; y < kHeight; y++) {
            int fy = y - kHorizon;             // rows below the horizon
            float road_w = fy * 0.55f;         // road half-width in pixels

            // The first ground row is a permanent dark purple baseline
            // either side of the road's tip pixels.
            Rgb plain = fy == 1 ? Rgb{44, 9, 66} : Rgb{10, 2, 26};
            for (int x = 0; x < kWidth; x++) {
                float dx = x - kCx;
                if (dx < 0) dx = -dx;
                m.set(x, y, dx <= road_w + 0.5f ? Rgb{16, 10, 30}    // road surface
                                                : plain);
            }

            // Road edges: bright cyan rails to the horizon.
            uint8_t edge_v = static_cast<uint8_t>(150 + fy * 7);
            m.set(static_cast<int>(lroundf(kCx - road_w)), y, Rgb::hsv(128, 190, edge_v));
            m.set(static_cast<int>(lroundf(kCx + road_w)), y, Rgb::hsv(128, 190, edge_v));
        }

        // --- center line: each dash is a simulated object. It is born as
        // a lone 2px mark one row below the road's tip, accelerates down
        // the road (screen speed grows with proximity, like perspective),
        // and stretches to a few rows long by the bottom. The next dash
        // only spawns once the newest one is 3+ rows below the origin, so
        // dashes always stay clearly separated.
        bool need_spawn = true;
        for (auto& ds : dashes_) {
            if (!ds.active) continue;
            ds.fy += (0.42f + 1.36f * ds.fy) * (dt / 1000.0f);
            if (ds.fy - dash_len(ds.fy) > 15.0f) { ds.active = false; continue; }
            if (ds.fy < 5.5f) need_spawn = false;
        }
        if (need_spawn) {
            for (auto& ds : dashes_) {
                if (ds.active) continue;
                ds.fy = 2.0f;
                ds.active = true;
                break;
            }
        }
        // Purple crossbar riding on each dash's leading row, plus a faint
        // trail on the rows it just passed. Bars exist only outside the
        // road so the cyan rails stay unbroken, and the parallax dots are
        // drawn later, i.e. on top. Trails go down first so a neighboring
        // dash's full-brightness bar is never overwritten by one.
        constexpr Rgb kBar{88, 18, 132};
        // frac is the trail falloff; on top of it the bar starts dark at
        // the horizon and brightens on its way down the screen.
        auto draw_bar = [&](int fy, int frac) {
            if (fy < 2 || fy > 15) return;
            int base = 80 + fy * 12;
            if (base > 255) base = 255;
            Rgb c = kBar.scaled(static_cast<uint8_t>((base * frac) >> 8));
            float bar_road_w = fy * 0.55f;
            for (int x = 0; x < kWidth; x++) {
                float dx = x - kCx;
                if (dx < 0) dx = -dx;
                if (dx > bar_road_w + 0.5f) m.set(x, kHorizon + fy, c);
            }
        };
        for (const auto& ds : dashes_) {
            if (!ds.active) continue;
            int head = static_cast<int>(lroundf(ds.fy));
            draw_bar(head - 1, 120);
            draw_bar(head - 2, 60);
        }
        for (const auto& ds : dashes_) {
            if (!ds.active) continue;
            int head = static_cast<int>(lroundf(ds.fy));
            int tail = static_cast<int>(lroundf(ds.fy - dash_len(ds.fy) + 1.0f));
            if (tail < 2) tail = 2;
            draw_bar(head, 255);
            for (int fy = tail; fy <= head; fy++) {
                if (fy > 15) break;
                int scale = 120 + fy * 9;
                Rgb c = Rgb{255, 235, 150}.scaled(scale > 255 ? 255 : scale);
                m.set(15, kHorizon + fy, c);
                m.set(16, kHorizon + fy, c);
            }
        }

        // --- roadside dot field: hold still, then all step down together
        // every kShiftMs. Each dot rides a straight line radiating from
        // the vanishing point (like the road edges), so it drifts outward
        // smoothly the whole way down -- subtly near the horizon, more
        // diagonally near the viewer -- and can never meet the road.
        if (t_ms >= next_shift_) {
            next_shift_ = t_ms + kShiftMs;
            for (auto& d : dots_) {
                d.fy++;
                if (d.fy > kHeight - 1 - kHorizon || dot_x(d) < 0 || dot_x(d) >= kWidth) {
                    spawn_dot(d, /*at_top=*/true);
                }
            }
        }
        for (const auto& d : dots_) {
            uint8_t v = static_cast<uint8_t>(80 + d.fy * 11);
            int x = dot_x(d);
            m.set(x, kHorizon + d.fy, Rgb::hsv(224, 230, v));
            // Near the viewer the object grows to two pixels wide.
            if (d.fy >= 11) m.set(x + (d.slope < 0 ? -1 : 1), kHorizon + d.fy, Rgb::hsv(224, 230, v));
        }
    }

private:
    static constexpr int kHorizon = 16;
    static constexpr int kStars = 10;
    static constexpr int kDots = 9;
    static constexpr uint32_t kShiftMs = 480;  // dot-field step cadence

    // A dot's path is x = center + slope * rows-below-horizon: a straight
    // ray out of the vanishing point. |slope| > the road's 0.55 keeps it
    // off the road at every depth.
    struct Dot {
        float slope;
        int8_t fy;
    };

    // The 1.6px base offset keeps every ray clear of the road's tip: even
    // at the first row a dot can never round onto the cyan edge pixels.
    static int dot_x(const Dot& d) {
        float off = 1.6f + (d.slope < 0 ? -d.slope : d.slope) * d.fy;
        return static_cast<int>(lroundf(d.slope < 0 ? 15.5f - off : 15.5f + off));
    }

    void reseed_star(int i) {
        star_x_[i] = esp_random() % kWidth;
        star_y_[i] = esp_random() % 4;  // stars only in the top four rows
        star_ph_[i] = esp_random() % 256;
        star_rate_[i] = 1.2f + (esp_random() % 24) / 10.0f;
    }

    static void spawn_dot(Dot& d, bool at_top) {
        // Gentle slopes only: at most ~2px of sideways travel per step,
        // and a life of many steps, so each dot reads as one object you
        // can follow from the horizon until it slides off screen.
        float mag = 0.7f + (esp_random() % 13) / 10.0f;  // 0.7 .. 1.9
        d.slope = esp_random() % 2 ? mag : -mag;
        int max_fy = static_cast<int>(14.0f / mag);
        if (max_fy > kHeight - 1 - kHorizon) max_fy = kHeight - 1 - kHorizon;
        d.fy = at_top ? 1 : 1 + esp_random() % max_fy;
    }

    // Dashes lengthen as they approach: 1 row at the origin, ~4 at the bottom.
    static float dash_len(float fy) { return 1.0f + (fy - 2.0f) * 0.22f; }

    uint8_t star_x_[kStars] = {};
    uint8_t star_y_[kStars] = {};
    uint8_t star_ph_[kStars] = {};
    float star_rate_[kStars] = {};
    struct Dash {
        float fy;
        bool active;
    };
    static constexpr int kMaxDashes = 5;

    Dot dots_[kDots] = {};
    Dash dashes_[kMaxDashes] = {};
    uint32_t next_shift_ = 0;
    uint32_t last_ = 0;
};

Animation* make_synthwave() {
    static Synthwave a;
    return &a;
}

}  // namespace anim
