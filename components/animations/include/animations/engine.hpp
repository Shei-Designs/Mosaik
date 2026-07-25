#pragma once

#include <cstddef>
#include <cstdint>

#include "panel/matrix.hpp"

namespace anim {

class Animation {
public:
    virtual ~Animation() = default;
    virtual const char* name() const = 0;
    // Called once when the animation becomes active (matrix is cleared first).
    virtual void reset(panel::Matrix& m) {}
    // Called every frame. t_ms is virtual time: it advances faster or slower
    // with the user's speed setting, so pace everything from it.
    virtual void step(panel::Matrix& m, uint32_t t_ms) = 0;
};

struct Status {
    int anim;
    uint8_t brightness;
    uint8_t speed;  // 128 == 1x
};

void engine_start();

int engine_animation_count();
const char* engine_animation_name(int i);

void engine_set_animation(int i);
void engine_set_brightness(uint8_t b);
void engine_set_speed(uint8_t s);
Status engine_status();

// Accepts a raw RGB frame (kNumLeds * 3 bytes, row-major x/y order),
// stores it and switches to the Pixel Art animation.
bool engine_show_frame(const uint8_t* rgb, size_t len);

}  // namespace anim
