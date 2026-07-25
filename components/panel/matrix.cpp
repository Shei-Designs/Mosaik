#include "panel/matrix.hpp"

#include "sdkconfig.h"

namespace panel {

Rgb Rgb::hsv(uint8_t h, uint8_t s, uint8_t v) {
    uint8_t region = h / 43;
    uint8_t rem = (h - region * 43) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;

    switch (region) {
        case 0: return {v, t, p};
        case 1: return {q, v, p};
        case 2: return {p, v, t};
        case 3: return {p, q, v};
        case 4: return {t, p, v};
        default: return {v, p, q};
    }
}

// Physical layout (verified against the real build with the test pattern):
// the four 8x32 panels stand vertically, side by side, each one an 8-wide
// by 32-tall column. They are chained left to right with short connectors,
// which forces every second panel to be mounted rotated 180 degrees.
//
// Within an unrotated panel the 256-LED strip enters at the BOTTOM-LEFT
// and snakes upward in horizontal rows of 8: row 0 (bottom) runs
// left->right, row 1 right->left, ... ending top-left. A rotated panel is
// the same thing turned 180 degrees, so its data enters at the TOP-RIGHT
// and the rows snake downward.
//
// Orientation deviations are Kconfig options; use the "Test Pattern"
// animation to verify after any physical change.
uint16_t xy_to_strip_index(int x, int y) {
#if CONFIG_PANEL_FLIP_X
    x = kWidth - 1 - x;
#endif
#if CONFIG_PANEL_FLIP_Y
    y = kHeight - 1 - y;
#endif

    int p = x / 8;   // which panel, 0 = leftmost
    int lx = x % 8;  // column within the panel, 0 = left

#if CONFIG_PANEL_CHAIN_REVERSED
    p = 3 - p;
#endif

#if CONFIG_PANEL_ALTERNATE_PANEL_ROTATION
    bool rotated = (p & 1) != 0;
#else
    bool rotated = false;
#endif

    int row, pos;
    if (!rotated) {
        row = kHeight - 1 - y;              // strip rows count from the bottom
        pos = (row & 1) ? (7 - lx) : lx;    // even rows run left->right
    } else {
        row = y;                            // rotated: rows count from the top
        pos = (y & 1) ? lx : (7 - lx);      // even rows run right->left
    }

    return p * 256 + row * 8 + pos;
}

}  // namespace panel
