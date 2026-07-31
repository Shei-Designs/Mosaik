#include <cstring>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Self-playing Snake. The AI breadth-first-searches a path to the food;
// when no path exists it chases its own tail to stay alive until one
// opens up. On death the snake flashes and the game restarts.
class Snake final : public Animation {
public:
    const char* name() const override { return "Snake"; }

    void reset(panel::Matrix&) override {
        restart();
        last_move_ = 0;
        dead_until_ = 0;
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        if (dead_until_) {
            // Death flash: body blinks red, then a fresh game starts.
            if (t_ms >= dead_until_) {
                restart();
                dead_until_ = 0;
            } else {
                draw(m, t_ms, (t_ms / 120) & 1);
                return;
            }
        }

        if (t_ms - last_move_ >= 90) {
            last_move_ = t_ms;
            move(t_ms);
        }
        draw(m, t_ms, false);
    }

private:
    static constexpr int kCells = kWidth * kHeight;

    static int idx(int x, int y) { return y * kWidth + x; }

    void restart() {
        std::memset(occupied_, 0, sizeof(occupied_));
        len_ = 4;
        head_ = 0;
        for (int i = 0; i < len_; i++) {
            body_[i] = idx(8 + i, 16);
            occupied_[body_[i]] = true;
        }
        head_ = len_ - 1;
        grow_ = 0;
        place_food();
    }

    void place_food() {
        do {
            food_ = esp_random() % kCells;
        } while (occupied_[food_]);
    }

    int tail_pos() const { return body_[(head_ + kCells - (len_ - 1)) % kCells]; }

    // The tail cell only counts as free if it moves away this tick, i.e.
    // the snake is not about to grow.
    int movable_tail() const { return grow_ == 0 ? tail_pos() : -2; }

    // BFS from the head to `target`; returns the first step's cell or -1.
    int path_step(int target) {
        int16_t prev[kCells];
        std::memset(prev, -1, sizeof(prev));
        int tail = movable_tail();
        int head = body_[head_];
        prev[head] = head;

        int qh = 0, qt = 0;
        queue_[qt++] = head;
        while (qh < qt) {
            int cur = queue_[qh++];
            if (cur == target) {
                while (prev[cur] != head) cur = prev[cur];
                return cur;
            }
            int x = cur % kWidth, y = cur / kWidth;
            const int nb[4] = {x > 0 ? cur - 1 : -1, x < kWidth - 1 ? cur + 1 : -1,
                               y > 0 ? cur - kWidth : -1, y < kHeight - 1 ? cur + kWidth : -1};
            for (int n : nb) {
                if (n < 0 || prev[n] != -1) continue;
                if (occupied_[n] && n != tail) continue;
                prev[n] = cur;
                queue_[qt++] = n;
            }
        }
        return -1;
    }

    void move(uint32_t t_ms) {
        int next = path_step(food_);
        if (next < 0) next = path_step(tail_pos());  // survive: follow the tail
        if (next < 0) {
            // Boxed in completely: last resort, any free neighbor.
            int cur = body_[head_];
            int x = cur % kWidth, y = cur / kWidth;
            const int nb[4] = {x > 0 ? cur - 1 : -1, x < kWidth - 1 ? cur + 1 : -1,
                               y > 0 ? cur - kWidth : -1, y < kHeight - 1 ? cur + kWidth : -1};
            for (int n : nb) {
                if (n >= 0 && (!occupied_[n] || n == movable_tail())) { next = n; break; }
            }
        }
        if (next < 0 || (occupied_[next] && next != movable_tail())) {
            dead_until_ = t_ms + 1200;
            return;
        }

        if (grow_ > 0 && len_ < kCells - 1) {
            grow_--;
            len_++;
        } else {
            occupied_[tail_pos()] = false;
        }
        head_ = (head_ + 1) % kCells;
        body_[head_] = next;
        occupied_[next] = true;

        if (next == food_) {
            grow_ += 3;
            place_food();
        }
    }

    void draw(panel::Matrix& m, uint32_t t_ms, bool flash) {
        m.clear();
        for (int i = 0; i < len_; i++) {
            int p = body_[(head_ + kCells - i) % kCells];
            uint8_t v = i == 0 ? 255 : static_cast<uint8_t>(200 - (150 * i) / len_);
            Rgb c = flash ? Rgb{v, 0, 0} : i == 0 ? Rgb{180, 255, 180} : Rgb::hsv(96, 255, v);
            m.set(p % kWidth, p / kWidth, c);
        }
        uint8_t pulse = static_cast<uint8_t>(170 + 85 * ((t_ms / 250) & 1));
        m.set(food_ % kWidth, food_ / kWidth, Rgb{pulse, 20, 20});
    }

    uint16_t body_[kCells] = {};   // ring buffer of cell indices
    uint16_t queue_[kCells] = {};  // BFS scratch
    bool occupied_[kCells] = {};
    int head_ = 0, len_ = 0, grow_ = 0;
    int food_ = 0;
    uint32_t last_move_ = 0;
    uint32_t dead_until_ = 0;
};

Animation* make_snake() {
    static Snake a;
    return &a;
}

}  // namespace anim
