#include <cmath>

#include "animations/engine.hpp"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Classic demoscene plasma: sum of drifting sine fields, mapped to hue.
class Plasma final : public Animation {
public:
    const char* name() const override { return "Plasma"; }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        float t = t_ms / 1000.0f;
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                float v = sinf(x * 0.35f + t) +
                          sinf(y * 0.28f - t * 0.8f) +
                          sinf((x + y) * 0.22f + t * 0.5f) +
                          sinf(sqrtf((x - 16.f) * (x - 16.f) + (y - 16.f) * (y - 16.f)) * 0.4f - t);
                uint8_t hue = (static_cast<int>((v + 4.0f) * 32.0f) + static_cast<int>(t * 10.0f)) & 0xFF;
                m.set(x, y, Rgb::hsv(hue, 255, 255));
            }
        }
    }
};

Animation* make_plasma() {
    static Plasma a;
    return &a;
}

}  // namespace anim
