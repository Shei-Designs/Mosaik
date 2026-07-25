#include <cstring>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Conway's Game of Life on a torus. Cell color encodes age: newborns are
// green, long-lived cells drift toward blue/violet. The population history
// is hashed to detect still lifes and short oscillators; the board reseeds
// itself when it stagnates.
class GameOfLife final : public Animation {
public:
    const char* name() const override { return "Game of Life"; }

    void reset(panel::Matrix&) override {
        reseed();
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        if (t_ms - last_step_ >= 120 || t_ms < last_step_) {
            last_step_ = t_ms;
            generation();
        }
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                uint8_t age = cells_[y][x];
                if (!age) {
                    m.set(x, y, {0, 0, 0});
                } else {
                    uint8_t hue = 96 + (age > 26 ? 130 : age * 5);
                    m.set(x, y, Rgb::hsv(hue, 255, 255));
                }
            }
        }
    }

private:
    void reseed() {
        std::memset(cells_, 0, sizeof(cells_));
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                if ((esp_random() & 0xFF) < 80) cells_[y][x] = 1;
            }
        }
        stagnant_ = 0;
        gens_ = 0;
    }

    void generation() {
        uint8_t next[kHeight][kWidth];
        int population = 0;
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                int n = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (!dx && !dy) continue;
                        int nx = (x + dx + kWidth) % kWidth;
                        int ny = (y + dy + kHeight) % kHeight;
                        if (cells_[ny][nx]) n++;
                    }
                }
                uint8_t age = cells_[y][x];
                if (age) {
                    next[y][x] = (n == 2 || n == 3) ? (age < 255 ? age + 1 : age) : 0;
                } else {
                    next[y][x] = (n == 3) ? 1 : 0;
                }
                if (next[y][x]) population++;
            }
        }
        std::memcpy(cells_, next, sizeof(cells_));
        gens_++;

        // Stagnation detection: population constant or period-2 for a while,
        // died out, or simply running very long.
        if (population == 0) {
            reseed();
            return;
        }
        if (population == pop_[0] || population == pop_[1]) {
            stagnant_++;
        } else {
            stagnant_ = 0;
        }
        pop_[1] = pop_[0];
        pop_[0] = population;
        if (stagnant_ > 90 || gens_ > 3000) reseed();
    }

    uint8_t cells_[kHeight][kWidth] = {};
    uint32_t last_step_ = 0;
    int pop_[2] = {-1, -1};
    int stagnant_ = 0;
    int gens_ = 0;
};

Animation* make_game_of_life() {
    static GameOfLife a;
    return &a;
}

}  // namespace anim
