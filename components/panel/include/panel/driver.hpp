#pragma once

#include <cstdint>
#include "panel/matrix.hpp"

namespace panel {

void driver_init();

// Applies gamma, global brightness and the current limiter, then transmits
// the frame. Blocks until the strip refresh is done (~31 ms for 1024 LEDs).
void driver_show(const Matrix& m, uint8_t brightness);

// Estimated current of the last transmitted frame, in mA (after limiting).
uint32_t driver_last_frame_ma();

}  // namespace panel
