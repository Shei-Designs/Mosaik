#include <cstring>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Langton's Ant on a torus, with two ants in different colors. Each ant
// turns right on an empty cell and left on a painted one, flips the cell,
// and steps forward — simple rules, famously chaotic-then-ordered behavior
// (the "highway" usually emerges after ~10k steps).
class Langton final : public Animation {
public:
    const char* name() const override { return "Langton's Ant"; }

    void reset(panel::Matrix&) override {
        std::memset(cells_, 0, sizeof(cells_));
        for (int i = 0; i < kAnts; i++) {
            ants_[i].x = 8 + esp_random() % 16;
            ants_[i].y = 8 + esp_random() % 16;
            ants_[i].dir = esp_random() % 4;
        }
        steps_ = 0;
        last_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        // ~12 ant-steps per frame keeps it lively without being a blur.
        uint32_t due = (t_ms - last_) / 3;
        if (t_ms < last_) due = 4;
        if (due > 40) due = 40;
        last_ = t_ms;

        for (uint32_t s = 0; s < due; s++) {
            for (int i = 0; i < kAnts; i++) step_ant(i);
            if (++steps_ > 60000) {
                reset(m);
                break;
            }
        }

        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                uint8_t c = cells_[y][x];
                m.set(x, y, c == 0 ? Rgb{0, 0, 0} : Rgb::hsv(c == 1 ? 20 : 190, 255, 140));
            }
        }
        for (int i = 0; i < kAnts; i++) {
            m.set(ants_[i].x, ants_[i].y, {255, 255, 255});
        }
    }

private:
    static constexpr int kAnts = 2;

    struct Ant {
        int x, y, dir;  // dir: 0=N 1=E 2=S 3=W
    };

    void step_ant(int i) {
        Ant& a = ants_[i];
        uint8_t& cell = cells_[a.y][a.x];
        if (cell == 0) {
            a.dir = (a.dir + 1) % 4;    // turn right
            cell = i + 1;               // paint with this ant's color
        } else {
            a.dir = (a.dir + 3) % 4;    // turn left
            cell = 0;
        }
        static constexpr int dx[] = {0, 1, 0, -1};
        static constexpr int dy[] = {-1, 0, 1, 0};
        a.x = (a.x + dx[a.dir] + kWidth) % kWidth;
        a.y = (a.y + dy[a.dir] + kHeight) % kHeight;
    }

    uint8_t cells_[kHeight][kWidth] = {};
    Ant ants_[kAnts] = {};
    uint32_t steps_ = 0;
    uint32_t last_ = 0;
};

Animation* make_langton() {
    static Langton a;
    return &a;
}

}  // namespace anim
