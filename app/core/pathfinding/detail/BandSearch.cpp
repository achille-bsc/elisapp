// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/BandSearch.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

#include "core/pathfinding/detail/GridStep.hpp"

namespace marspath::detail {

bool BandSearch::passable(CoarseGrid& grid, CoarseGrid::Index idx, double low, double high) {
    grid.ensure(idx);
    if (grid.blocked(idx)) {
        return false;
    }
    if (idx == grid.startCell() || idx == grid.goalCell()) {
        return true;
    }
    const double z = grid.elevation(idx);
    return z >= low && z <= high;
}

bool BandSearch::reachable(CoarseGrid& grid, double low, double high) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    seen_.assign((grid.size() + 63U) / 64U, 0U);
    queue_.clear();
    const auto mark = [this](std::uint32_t i) { seen_[i >> 6U] |= std::uint64_t{1} << (i & 63U); };
    const auto isSeen = [this](std::uint32_t i) { return ((seen_[i >> 6U] >> (i & 63U)) & 1U) != 0U; };

    queue_.push_back(grid.startCell());
    mark(grid.startCell());
    for (std::size_t head = 0; head < queue_.size(); ++head) {
        const CoarseGrid::Index cur = queue_[head];
        if (cur == grid.goalCell()) {
            return true;
        }
        for (std::size_t k = 0; k < kGridSteps.size(); ++k) {
            const auto nb = grid.neighbor(cur, k);
            if (!nb || isSeen(*nb) || !passable(grid, *nb, low, high)) {
                continue;
            }
            if (kGridSteps[k].di != 0 && kGridSteps[k].dj != 0 &&
                (!passable(grid, grid.cornerCell(*nb, cur), -kInf, kInf) ||
                 !passable(grid, grid.cornerCell(cur, *nb), -kInf, kInf))) {
                continue;  // no diagonal move that cuts a blocked corner
            }
            mark(*nb);
            queue_.push_back(*nb);
        }
    }
    return false;
}

std::optional<AltitudeBand> BandSearch::narrowest(CoarseGrid& grid, const ElevationParams& params) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    if (!reachable(grid, -kInf, kInf)) {
        return std::nullopt;
    }
    const double z_a = std::min(grid.elevation(grid.startCell()), grid.elevation(grid.goalCell()));
    const double z_b = std::max(grid.elevation(grid.startCell()), grid.elevation(grid.goalCell()));
    const double step = std::max(1.0, params.band_step_m);

    // 1) Highest feasible floor (no ceiling).
    double low = z_a;
    if (!reachable(grid, low, kInf)) {
        double ok = z_a - 250.0;
        while (!reachable(grid, ok, kInf)) {
            ok = z_a - 2.0 * (z_a - ok);
        }
        double bad = z_a;
        while (bad - ok > step) {
            const double mid = 0.5 * (ok + bad);
            (reachable(grid, mid, kInf) ? ok : bad) = mid;
        }
        low = ok;
    }
    // 2) Lowest feasible ceiling for that floor.
    double hi_ok = z_b + 250.0;
    while (!reachable(grid, low, hi_ok)) {
        hi_ok = z_b + 2.0 * (hi_ok - z_b);
    }
    double hi_bad = z_b - step;
    while (hi_ok - hi_bad > step) {
        const double mid = 0.5 * (hi_ok + hi_bad);
        (reachable(grid, low, mid) ? hi_ok : hi_bad) = mid;
    }
    // 3) Sweep: lower the floor step by step, lowering the ceiling as far as possible.
    double high = hi_ok;
    double best_low = low;
    double best_high = high;
    while (z_b - (low - step) < best_high - best_low) {
        low -= step;
        while (high - step >= z_b && reachable(grid, low, high - step)) {
            high -= step;
        }
        if (high - low < best_high - best_low) {
            best_low = low;
            best_high = high;
        }
    }
    const double extra = (best_high - best_low) * (params.band_slack - 1.0) / 2.0 + params.band_extra_m;
    return AltitudeBand{best_low - extra, best_high + extra};
}

}  // namespace marspath::detail
