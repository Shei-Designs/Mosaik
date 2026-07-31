#include <cmath>

#include "animations/engine.hpp"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Endless dive into the Mandelbrot set's seahorse valley. The zoom only
// ever goes inward; when single-precision floats run out of pixels the
// image crossfades directly into the zoomed-out opening view of the next
// dive (rendered as a second field and alpha-blended), so the descent
// never visibly pauses or dips to black. Each dive rotates the palette.
class FractalZoom final : public Animation {
public:
    const char* name() const override { return "Fractal Zoom"; }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        float total = t_ms * 0.00035f;
        float depth = fmodf(total, kMaxDepth);
        int dive = static_cast<int>(total / kMaxDepth);
        uint8_t hue_shift = (t_ms / 90) & 0xFF;

        View cur = make_view(depth, dive);

        // Crossfade window at the end of the dive: blend toward the next
        // dive's view at depth - kMaxDepth (i.e. still zooming in from
        // even further out), so at the handoff the new dive is exactly at
        // its starting zoom and the motion reads as continuous.
        float xf = 0.0f;
        View next{};
        if (depth > kMaxDepth - kFadeW) {
            float a = (depth - (kMaxDepth - kFadeW)) / kFadeW;
            xf = a * a * (3.0f - 2.0f * a);  // smoothstep
            next = make_view(depth - kMaxDepth, dive + 1);
        }

        for (int py = 0; py < kHeight; py++) {
            for (int px = 0; px < kWidth; px++) {
                Rgb c = shade(iterate(px, py, cur), cur, hue_shift);
                if (xf > 0.0f) {
                    Rgb n = shade(iterate(px, py, next), next, hue_shift);
                    c.r = static_cast<uint8_t>(c.r + (n.r - c.r) * xf);
                    c.g = static_cast<uint8_t>(c.g + (n.g - c.g) * xf);
                    c.b = static_cast<uint8_t>(c.b + (n.b - c.b) * xf);
                }
                m.set(px, py, c);
            }
        }
    }

private:
    struct View {
        float x0, y0, step;
        int max_iter;
        uint8_t pal;
    };

    static View make_view(float depth, int dive) {
        View v;
        float scale = 3.2f * expf(-depth);
        v.step = scale / kWidth;
        v.x0 = kCenterX - v.step * (kWidth - 1) * 0.5f;
        v.y0 = kCenterY - v.step * (kHeight - 1) * 0.5f;
        // More iterations as detail shrinks, capped to hold the frame
        // rate (negative depth = the wide opening view, minimum detail).
        int mi = 24 + static_cast<int>((depth > 0.0f ? depth : 0.0f) * 7.0f);
        v.max_iter = mi > 96 ? 96 : mi;
        v.pal = static_cast<uint8_t>(dive * 47);
        return v;
    }

    static int iterate(int px, int py, const View& v) {
        float cr = v.x0 + px * v.step;
        float ci = v.y0 + py * v.step;
        float zr = 0.0f, zi = 0.0f;
        int it = 0;
        while (it < v.max_iter) {
            float zr2 = zr * zr, zi2 = zi * zi;
            if (zr2 + zi2 > 4.0f) break;
            zi = 2.0f * zr * zi + ci;
            zr = zr2 - zi2 + cr;
            it++;
        }
        return it;
    }

    static Rgb shade(int it, const View& v, uint8_t hue_shift) {
        if (it >= v.max_iter) return Rgb{0, 0, 0};  // inside the set
        uint8_t hue = static_cast<uint8_t>(it * 5 + hue_shift + v.pal);
        uint8_t val = it < 3 ? 90 : 255;  // dim the far-outside wash
        return Rgb::hsv(hue, 255, val);
    }

    // Seahorse-valley boundary point: detail at every depth float can reach.
    static constexpr float kCenterX = -0.74364388f;
    static constexpr float kCenterY = 0.13182590f;
    static constexpr float kMaxDepth = 10.5f;  // ~e^10.5 zoom, float's limit here
    static constexpr float kFadeW = 0.45f;     // crossfade window, in depth units
};

Animation* make_fractal_zoom() {
    static FractalZoom a;
    return &a;
}

}  // namespace anim
