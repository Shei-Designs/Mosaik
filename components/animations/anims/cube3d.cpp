#include <cmath>

#include "animations/engine.hpp"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// A wireframe cube tumbling on two axes, perspective-projected. Edges are
// drawn additively over a faded previous frame, leaving a short ghost
// trail; the hue drifts slowly around the wheel.
class Cube3d final : public Animation {
public:
    const char* name() const override { return "Cube"; }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        float a = t_ms * 0.00071f;  // yaw
        float b = t_ms * 0.00047f;  // pitch
        float ca = cosf(a), sa = sinf(a);
        float cb = cosf(b), sb = sinf(b);
        uint8_t hue = (t_ms / 60) & 0xFF;

        m.fade(90);

        float px[8], py[8];
        for (int i = 0; i < 8; i++) {
            float x = (i & 1) ? 1.0f : -1.0f;
            float y = (i & 2) ? 1.0f : -1.0f;
            float z = (i & 4) ? 1.0f : -1.0f;
            // Rotate around Y, then X.
            float x1 = x * ca + z * sa;
            float z1 = -x * sa + z * ca;
            float y1 = y * cb - z1 * sb;
            float z2 = y * sb + z1 * cb;
            // Gentle breathing so the cube periodically fills the panel.
            float s = 1.05f + 0.15f * sinf(t_ms * 0.00031f);
            float persp = 16.5f * s / (z2 + 3.4f);
            px[i] = 15.5f + x1 * persp;
            py[i] = 15.5f + y1 * persp;
        }

        static constexpr uint8_t kEdges[12][2] = {
            {0, 1}, {2, 3}, {4, 5}, {6, 7},  // x-aligned
            {0, 2}, {1, 3}, {4, 6}, {5, 7},  // y-aligned
            {0, 4}, {1, 5}, {2, 6}, {3, 7},  // z-aligned
        };
        for (int e = 0; e < 12; e++) {
            // Slightly different hue per axis group reads as depth.
            uint8_t h = hue + (e / 4) * 24;
            line(m, px[kEdges[e][0]], py[kEdges[e][0]], px[kEdges[e][1]], py[kEdges[e][1]],
                 Rgb::hsv(h, 220, 255));
        }
    }

private:
    static void line(panel::Matrix& m, float x0f, float y0f, float x1f, float y1f, Rgb c) {
        int x0 = static_cast<int>(lroundf(x0f)), y0 = static_cast<int>(lroundf(y0f));
        int x1 = static_cast<int>(lroundf(x1f)), y1 = static_cast<int>(lroundf(y1f));
        int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
        int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            m.add(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
};

Animation* make_cube3d() {
    static Cube3d a;
    return &a;
}

}  // namespace anim
