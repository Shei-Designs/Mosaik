#include <cmath>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Metaball lava lamp: warm blobs rise, sink and merge over a dark purple
// glass. Each pixel sums the blobs' inverse-square field; the threshold
// band gives the goo its soft rounded outline.
class LavaLamp final : public Animation {
public:
    const char* name() const override { return "Lava Lamp"; }

    void reset(panel::Matrix&) override {
        for (auto& b : blobs_) {
            b.cx = 4.0f + esp_random() % 24;
            b.phase = (esp_random() % 628) / 100.0f;
            b.period = 9.0f + (esp_random() % 120) / 10.0f;   // seconds per cycle
            b.r = 2.6f + (esp_random() % 25) / 10.0f;
            b.wobble = 1.5f + (esp_random() % 25) / 10.0f;
        }
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        float t = t_ms / 1000.0f;

        float bx[kBlobs], by[kBlobs], r2[kBlobs];
        for (int i = 0; i < kBlobs; i++) {
            const Blob& b = blobs_[i];
            float ph = t * 6.2832f / b.period + b.phase;
            // Cosine hover between the heated bottom and the cool top; a
            // slow horizontal wobble keeps blobs meeting each other.
            by[i] = 16.0f + 13.0f * cosf(ph);
            bx[i] = b.cx + b.wobble * sinf(t * 0.35f + b.phase * 2.0f);
            // Blobs swell near the bottom (heat) and shrink at the top.
            float swell = 1.0f + 0.25f * cosf(ph);
            r2[i] = b.r * b.r * swell * swell;
        }

        // The goo's hue crawls around the whole wheel (~100 s per lap), so
        // the lava slowly shifts red -> orange -> green -> blue -> purple.
        uint8_t hue_base = (t_ms / 400) & 0xFF;

        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                float f = 0.0f;
                for (int i = 0; i < kBlobs; i++) {
                    float dx = x - bx[i], dy = y - by[i];
                    f += r2[i] / (dx * dx + dy * dy + 1.0f);
                }
                m.set(x, y, shade(f, y, hue_base));
            }
        }
    }

private:
    static Rgb shade(float f, int y, uint8_t hue_base) {
        // Background: near-black purple, slightly warmer toward the bottom.
        if (f < 0.55f) {
            uint8_t warm = static_cast<uint8_t>(y / 3);
            return Rgb{static_cast<uint8_t>(10 + warm / 2), 0, static_cast<uint8_t>(22 + warm / 4)};
        }
        // Rim -> body -> hot core: fully saturated goo whose hue leans a
        // little warmer toward the middle, with a bleached-hot center.
        float u = f > 2.2f ? 1.0f : (f - 0.55f) / (2.2f - 0.55f);
        uint8_t hue = static_cast<uint8_t>(hue_base + u * 28.0f);
        uint8_t sat = f > 3.0f ? 150 : 255;                     // core bleaches out
        uint8_t val = static_cast<uint8_t>(90 + u * 165.0f);
        return Rgb::hsv(hue, sat, val);
    }

    struct Blob {
        float cx, phase, period, r, wobble;
    };

    static constexpr int kBlobs = 6;
    Blob blobs_[kBlobs] = {};
};

Animation* make_lava_lamp() {
    static LavaLamp a;
    return &a;
}

}  // namespace anim
