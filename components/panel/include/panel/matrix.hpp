#pragma once

#include <cstdint>
#include <cstring>

namespace panel {

inline constexpr int kWidth = 32;
inline constexpr int kHeight = 32;
inline constexpr int kNumLeds = kWidth * kHeight;

struct Rgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    // Fast spectrum HSV -> RGB, all channels 0..255.
    static Rgb hsv(uint8_t h, uint8_t s, uint8_t v);

    Rgb scaled(uint8_t scale) const {
        return Rgb{static_cast<uint8_t>((r * scale) >> 8),
                   static_cast<uint8_t>((g * scale) >> 8),
                   static_cast<uint8_t>((b * scale) >> 8)};
    }
};

// 32x32 framebuffer in logical x/y space, origin top-left. The physical
// serpentine strip order is only applied when the driver pushes a frame.
class Matrix {
public:
    void clear() { std::memset(px_, 0, sizeof(px_)); }

    void fill(Rgb c) {
        for (auto& p : px_) p = c;
    }

    // Scale every pixel by (256 - amount) / 256; amount 0 keeps, 255 clears.
    void fade(uint8_t amount) {
        uint8_t keep = 255 - amount;
        for (auto& p : px_) p = p.scaled(keep);
    }

    void set(int x, int y, Rgb c) {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        px_[y * kWidth + x] = c;
    }

    // Saturating additive blend.
    void add(int x, int y, Rgb c) {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        Rgb& p = px_[y * kWidth + x];
        int r = p.r + c.r, g = p.g + c.g, b = p.b + c.b;
        p.r = r > 255 ? 255 : r;
        p.g = g > 255 ? 255 : g;
        p.b = b > 255 ? 255 : b;
    }

    Rgb get(int x, int y) const {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return Rgb{};
        return px_[y * kWidth + x];
    }

    const Rgb* raw() const { return px_; }
    Rgb* raw() { return px_; }

private:
    Rgb px_[kNumLeds] = {};
};

// Logical (x, y) -> physical LED index along the chained serpentine panels.
uint16_t xy_to_strip_index(int x, int y);

}  // namespace panel
