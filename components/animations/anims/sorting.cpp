#include <algorithm>
#include <vector>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Sorting visualizer: 32 bars, one per column, height = value. Each run
// records the algorithm's compares and swaps into a trace, then replays it
// so every comparison is visible. Cycles bubble -> selection -> insertion ->
// quicksort, reshuffling in between.
class Sorting final : public Animation {
public:
    const char* name() const override { return "Sorting"; }

    void reset(panel::Matrix&) override {
        algo_ = 0;
        start_run();
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        if (t_ms - last_ >= 45 || t_ms < last_) {
            last_ = t_ms;
            advance();
        }

        m.clear();
        for (int x = 0; x < kWidth; x++) {
            int v = display_[x];
            bool hot = (pos_ < trace_.size()) &&
                       (trace_[pos_].i == x || trace_[pos_].j == x);
            for (int y = 0; y < v; y++) {
                Rgb c = hot ? Rgb{255, 255, 255} : Rgb::hsv(v * 7, 255, 200);
                m.set(x, kHeight - 1 - y, c);
            }
        }
    }

private:
    struct Op {
        uint8_t i, j;
        bool swap;
    };

    void start_run() {
        for (int i = 0; i < kWidth; i++) work_[i] = i + 1;
        for (int i = kWidth - 1; i > 0; i--) {
            std::swap(work_[i], work_[esp_random() % (i + 1)]);
        }
        std::copy(work_, work_ + kWidth, display_);
        trace_.clear();
        pos_ = 0;
        done_hold_ = 0;
        record();
    }

    void cmp(int i, int j) { trace_.push_back({(uint8_t)i, (uint8_t)j, false}); }
    void swp(int i, int j) {
        trace_.push_back({(uint8_t)i, (uint8_t)j, true});
        std::swap(work_[i], work_[j]);
    }

    void record() {
        switch (algo_) {
            case 0:  // bubble
                for (int n = kWidth; n > 1; n--) {
                    for (int i = 0; i < n - 1; i++) {
                        cmp(i, i + 1);
                        if (work_[i] > work_[i + 1]) swp(i, i + 1);
                    }
                }
                break;
            case 1:  // selection
                for (int i = 0; i < kWidth - 1; i++) {
                    int mn = i;
                    for (int j = i + 1; j < kWidth; j++) {
                        cmp(mn, j);
                        if (work_[j] < work_[mn]) mn = j;
                    }
                    if (mn != i) swp(i, mn);
                }
                break;
            case 2:  // insertion (as adjacent swaps so the motion is visible)
                for (int i = 1; i < kWidth; i++) {
                    for (int j = i; j > 0; j--) {
                        cmp(j - 1, j);
                        if (work_[j - 1] > work_[j]) swp(j - 1, j);
                        else break;
                    }
                }
                break;
            default:  // quicksort (Lomuto)
                quicksort(0, kWidth - 1);
                break;
        }
    }

    void quicksort(int lo, int hi) {
        if (lo >= hi) return;
        int pivot = work_[hi];
        int i = lo;
        for (int j = lo; j < hi; j++) {
            cmp(j, hi);
            if (work_[j] < pivot) {
                if (i != j) swp(i, j);
                i++;
            }
        }
        if (i != hi) swp(i, hi);
        quicksort(lo, i - 1);
        quicksort(i + 1, hi);
    }

    void advance() {
        if (pos_ < trace_.size()) {
            const Op& op = trace_[pos_];
            if (op.swap) std::swap(display_[op.i], display_[op.j]);
            pos_++;
            return;
        }
        // Sorted: hold the finished picture briefly, then next algorithm.
        if (++done_hold_ > 40) {
            algo_ = (algo_ + 1) % 4;
            start_run();
        }
    }

    int algo_ = 0;
    uint8_t work_[kWidth];
    uint8_t display_[kWidth];
    std::vector<Op> trace_;
    size_t pos_ = 0;
    int done_hold_ = 0;
    uint32_t last_ = 0;
};

Animation* make_sorting() {
    static Sorting a;
    return &a;
}

}  // namespace anim
