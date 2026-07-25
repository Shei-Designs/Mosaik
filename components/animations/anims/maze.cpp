#include <cstring>
#include <vector>

#include "animations/engine.hpp"
#include "esp_random.h"

namespace anim {

using panel::kHeight;
using panel::kWidth;
using panel::Rgb;

// Maze demo in three acts: carve a maze with randomized depth-first search,
// flood it with BFS from the entrance (cells colored by distance), then
// light up the shortest path to the exit. 16x16 cells drawn on a 31x31
// pixel grid (odd pixels are walls).
class Maze final : public Animation {
public:
    const char* name() const override { return "Maze"; }

    void reset(panel::Matrix&) override {
        start_maze();
    }

    void step(panel::Matrix& m, uint32_t t_ms) override {
        if (t_ms - last_ >= 25 || t_ms < last_) {
            last_ = t_ms;
            advance();
        }
        draw(m);
    }

private:
    static constexpr int N = 16;                 // cells per side
    static constexpr int P = 2 * N - 1;          // pixel grid (31)
    enum class Phase { kCarve, kFlood, kPath, kHold };

    static int px(int cx) { return cx * 2; }

    void start_maze() {
        std::memset(open_, 0, sizeof(open_));
        std::memset(visited_, 0, sizeof(visited_));
        std::memset(dist_, -1, sizeof(dist_));
        stack_.clear();
        frontier_.clear();
        path_.clear();
        stack_.push_back({0, 0});
        visited_[0][0] = true;
        phase_ = Phase::kCarve;
        hold_ = 0;
    }

    void advance() {
        switch (phase_) {
            case Phase::kCarve: carve_step(); break;
            case Phase::kFlood: flood_step(); break;
            case Phase::kPath: path_step(); break;
            case Phase::kHold:
                if (++hold_ > 80) start_maze();
                break;
        }
    }

    // One step of iterative randomized DFS ("recursive backtracker").
    void carve_step() {
        if (stack_.empty()) {
            std::memset(dist_, -1, sizeof(dist_));
            dist_[0][0] = 0;
            frontier_.push_back({0, 0});
            phase_ = Phase::kFlood;
            return;
        }
        auto [cx, cy] = stack_.back();
        static constexpr int dx[] = {1, -1, 0, 0};
        static constexpr int dy[] = {0, 0, 1, -1};
        int order[4] = {0, 1, 2, 3};
        for (int i = 3; i > 0; i--) std::swap(order[i], order[esp_random() % (i + 1)]);

        for (int k = 0; k < 4; k++) {
            int d = order[k];
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || nx >= N || ny < 0 || ny >= N || visited_[ny][nx]) continue;
            visited_[ny][nx] = true;
            open_[py_between(cy, ny)][px_between(cx, nx)] = true;
            stack_.push_back({nx, ny});
            return;
        }
        stack_.pop_back();
    }

    static int px_between(int a, int b) { return a + b; }  // 2a and 2b differ by 2; wall at a+b
    static int py_between(int a, int b) { return a + b; }

    // One BFS ring per frame: expands the frontier by a single distance level.
    void flood_step() {
        if (dist_[N - 1][N - 1] >= 0 || frontier_.empty()) {
            trace_path();
            phase_ = Phase::kPath;
            return;
        }
        std::vector<std::pair<int, int>> next;
        static constexpr int dx[] = {1, -1, 0, 0};
        static constexpr int dy[] = {0, 0, 1, -1};
        for (auto [cx, cy] : frontier_) {
            for (int d = 0; d < 4; d++) {
                int nx = cx + dx[d], ny = cy + dy[d];
                if (nx < 0 || nx >= N || ny < 0 || ny >= N) continue;
                if (dist_[ny][nx] >= 0) continue;
                if (!open_[py_between(cy, ny)][px_between(cx, nx)]) continue;
                dist_[ny][nx] = dist_[cy][cx] + 1;
                next.push_back({nx, ny});
            }
        }
        frontier_ = std::move(next);
    }

    void trace_path() {
        path_.clear();
        int cx = N - 1, cy = N - 1;
        if (dist_[cy][cx] < 0) return;
        path_.push_back({cx, cy});
        static constexpr int dx[] = {1, -1, 0, 0};
        static constexpr int dy[] = {0, 0, 1, -1};
        while (cx != 0 || cy != 0) {
            for (int d = 0; d < 4; d++) {
                int nx = cx + dx[d], ny = cy + dy[d];
                if (nx < 0 || nx >= N || ny < 0 || ny >= N) continue;
                if (!open_[py_between(cy, ny)][px_between(cx, nx)]) continue;
                if (dist_[ny][nx] == dist_[cy][cx] - 1) {
                    cx = nx;
                    cy = ny;
                    path_.push_back({cx, cy});
                    break;
                }
            }
        }
        shown_ = 0;
    }

    void path_step() {
        if (shown_ < path_.size()) shown_ += 1;
        else phase_ = Phase::kHold;
    }

    void draw(panel::Matrix& m) {
        m.clear();
        int maxd = dist_[N - 1][N - 1] > 0 ? dist_[N - 1][N - 1] : 2 * N;

        for (int cy = 0; cy < N; cy++) {
            for (int cx = 0; cx < N; cx++) {
                if (!visited_[cy][cx]) continue;
                // Bright enough to survive gamma correction; values below
                // ~50 are nearly invisible on the panel.
                Rgb c{70, 70, 95};
                if (dist_[cy][cx] >= 0 && phase_ != Phase::kCarve) {
                    c = Rgb::hsv(160 + (dist_[cy][cx] * 90) / maxd, 255, 170);
                }
                m.set(px(cx), px(cy), c);
            }
        }
        // Carved openings between cells.
        for (int y = 0; y < P; y++) {
            for (int x = 0; x < P; x++) {
                if (!open_[y][x]) continue;
                Rgb c{70, 70, 95};
                if (phase_ != Phase::kCarve) c = {30, 30, 46};
                m.set(x, y, c);
            }
        }
        // Active DFS head while carving.
        if (phase_ == Phase::kCarve && !stack_.empty()) {
            auto [cx, cy] = stack_.back();
            m.set(px(cx), px(cy), {255, 180, 0});
        }
        // Solved path lights up gold from the exit backwards.
        if (phase_ == Phase::kPath || phase_ == Phase::kHold) {
            size_t n = phase_ == Phase::kHold ? path_.size() : shown_;
            for (size_t i = 0; i < n && i < path_.size(); i++) {
                auto [cx, cy] = path_[i];
                m.set(px(cx), px(cy), {255, 200, 30});
                if (i + 1 < n && i + 1 < path_.size()) {
                    auto [nx, ny] = path_[i + 1];
                    m.set(px_between(cx, nx) + (cx == nx ? 0 : 0), py_between(cy, ny), {200, 150, 20});
                }
            }
        }
        // Entrance / exit markers.
        m.set(0, 0, {0, 255, 0});
        m.set(px(N - 1), px(N - 1), {255, 0, 0});
    }

    bool open_[P][P];        // openings between adjacent cells (odd coords)
    bool visited_[N][N];
    int16_t dist_[N][N];
    std::vector<std::pair<int, int>> stack_;
    std::vector<std::pair<int, int>> frontier_;
    std::vector<std::pair<int, int>> path_;
    size_t shown_ = 0;
    Phase phase_ = Phase::kCarve;
    int hold_ = 0;
    uint32_t last_ = 0;
};

Animation* make_maze() {
    static Maze a;
    return &a;
}

}  // namespace anim
