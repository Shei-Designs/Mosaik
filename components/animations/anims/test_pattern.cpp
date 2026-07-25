#include "animations/engine.hpp"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Mapping verification pattern:
//  - corner markers: red = (0,0) top-left, green = top-right,
//    blue = bottom-left, white = bottom-right
//  - dim columns mark the boundaries between the four vertical panels
//  - a cyan pixel scans row-major (left->right, top->bottom)
// If the scan jumps around or the corners land in the wrong place, adjust
// the mapping options in menuconfig ("Panel mapping / orientation").
class TestPattern final : public Animation {
public:
    const char* name() const override { return "Test Pattern"; }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        m.clear();

        for (int x = 8; x < kWidth; x += 8) {
            for (int y = 0; y < kHeight; y++) m.set(x, y, {12, 12, 12});
        }

        m.set(0, 0, {255, 0, 0});
        m.set(kWidth - 1, 0, {0, 255, 0});
        m.set(0, kHeight - 1, {0, 0, 255});
        m.set(kWidth - 1, kHeight - 1, {255, 255, 255});

        int pos = (t_ms / 40) % (kWidth * kHeight);
        m.set(pos % kWidth, pos / kWidth, {0, 255, 255});
    }
};

Animation* make_test_pattern() {
    static TestPattern a;
    return &a;
}

}  // namespace anim
