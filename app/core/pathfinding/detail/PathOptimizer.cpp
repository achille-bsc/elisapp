// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/PathOptimizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace marspath::detail {

double PathOptimizer::segmentCost(const PathNode& u, const PathNode& v) const noexcept {
    const double d = norm(v.pos() - u.pos());
    if (d < 1e-6) {
        return 0.0;
    }
    const double zu = u.z;
    const double zv = v.z;
    double deviation_sq = 0.0;
    if (cost_.usesDeviation()) {
        const double du = zu - reference_.at(u.pos());
        const double dv = zv - reference_.at(v.pos());
        deviation_sq = 0.5 * (du * du + dv * dv);
    }
    double c = cost_.edge(d, zv - zu, deviation_sq);
    if (band_.active()) {
        c += cost_.bandPenalty(std::max(band_.excess(zu), band_.excess(zv)), d);
    }
    return c;
}

double PathOptimizer::rangeCost(const Path& path, std::size_t lo, std::size_t hi) const noexcept {
    double c = 0.0;
    for (std::size_t k = lo; k < hi; ++k) {
        c += segmentCost(path[k], path[k + 1]);
    }
    for (std::size_t k = std::max<std::size_t>(lo, 1); k <= hi && k + 1 < path.size(); ++k) {
        c += cost_.turn(path[k].pos() - path[k - 1].pos(), path[k + 1].pos() - path[k].pos());
    }
    return c;
}

void PathOptimizer::applyBump(Path& path, std::size_t lo, std::size_t hi, std::size_t center, int window,
                              Vec2 move) const {
    for (std::size_t j = lo; j <= hi; ++j) {
        const double distance = std::fabs(static_cast<double>(j) - static_cast<double>(center));
        const double weight = 1.0 - distance / (window + 1.0);
        const Vec2 p = backup_[j - lo].pos() + move * weight;
        path[j] = PathNode::at(p, probe_.elevation(p));
    }
}

bool PathOptimizer::rangeClear(const Path& path, std::size_t lo, std::size_t hi) const {
    for (std::size_t j = lo + 1; j < hi; ++j) {
        if (!probe_.isClear(path[j].pos())) {
            return false;
        }
    }
    for (std::size_t j = lo; j < hi; ++j) {
        if (!probe_.segmentClear(path[j].pos(), path[j + 1].pos())) {
            return false;
        }
    }
    return true;
}

bool PathOptimizer::optimize(Path& path, double initial_step, const Deadline& deadline) {
    if (path.size() < 3) {
        return true;
    }
    backup_.reserve(2U * params_.optim_window + 1U);
    std::vector<int> windows;
    for (int w = params_.optim_window; w > 0; w /= 2) {
        windows.push_back(w);
    }
    windows.push_back(0);

    for (const int w : windows) {
        for (double delta = initial_step; delta >= params_.optim_min_step_m; delta *= 0.5) {
            for (std::uint16_t pass = 0; pass < params_.optim_max_passes; ++pass) {
                if (deadline.expired()) {
                    return false;
                }
                bool improved = false;
                const std::size_t n = path.size();
                for (std::size_t i = 1; i + 1 < n; ++i) {
                    if ((i & 31U) == 0U && deadline.expired()) {
                        return false;
                    }
                    const std::size_t lo = i > static_cast<std::size_t>(w) ? i - static_cast<std::size_t>(w) : 1U;
                    const std::size_t hi = std::min(n - 2, i + static_cast<std::size_t>(w));
                    const std::size_t c_lo = lo - 1;
                    const std::size_t c_hi = hi + 1;
                    backup_.assign(path.begin() + static_cast<std::ptrdiff_t>(lo),
                                   path.begin() + static_cast<std::ptrdiff_t>(hi) + 1);
                    const double c0 = rangeCost(path, c_lo, c_hi);
                    const Vec2 lateral = perp(normalized(path[i + 1].pos() - path[i - 1].pos()));

                    // 1) Cost of both lateral moves (elevation queries only).
                    const Vec2 moves[2] = {lateral * delta, lateral * -delta};
                    double costs[2];
                    for (int k = 0; k < 2; ++k) {
                        applyBump(path, lo, hi, i, w, moves[k]);
                        costs[k] = rangeCost(path, c_lo, c_hi);
                    }
                    const int order[2] = {costs[0] <= costs[1] ? 0 : 1, costs[0] <= costs[1] ? 1 : 0};
                    // 2) Hazard checks (expensive) only for an improving move.
                    bool accepted = false;
                    for (const int k : order) {
                        if (costs[k] >= c0 - params_.optim_min_gain) {
                            break;
                        }
                        applyBump(path, lo, hi, i, w, moves[k]);
                        if (rangeClear(path, c_lo, c_hi)) {
                            accepted = true;
                            break;
                        }
                    }
                    if (accepted) {
                        improved = true;
                    } else {
                        std::copy(backup_.begin(), backup_.end(), path.begin() + static_cast<std::ptrdiff_t>(lo));
                    }
                }
                if (!improved) {
                    break;
                }
            }
        }
    }
    return true;
}

}  // namespace marspath::detail
