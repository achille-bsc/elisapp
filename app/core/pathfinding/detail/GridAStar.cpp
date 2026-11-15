// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/GridAStar.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numbers>

#include "core/pathfinding/detail/GridStep.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

bool GridAStar::run(CoarseGrid& grid, const Query& q, std::vector<CoarseGrid::Index>& out) {
    const std::size_t n = grid.size();
    const CoarseGrid::Index start = grid.startCell();
    const CoarseGrid::Index goal = grid.goalCell();
    const Vec2 goal_center = grid.center(goal);
    const double z_goal = grid.elevation(goal);
    const double diagonal = grid.cellSize() * std::numbers::sqrt2;
    const bool use_deviation = q.cost.usesDeviation();
    const double partial = q.cost.partialBlockPenalty();
    const double caution = q.cost.cautionPenalty();

    g_.assign(n, std::numeric_limits<float>::infinity());
    meta_.assign(n, 0U);
    heap_.clear();
    const auto greater = [](const HeapEntry& a, const HeapEntry& b) { return a.f > b.f; };
    const auto heuristic = [&](CoarseGrid::Index idx) {
        return q.cost.heuristic(norm(grid.center(idx) - goal_center), z_goal - grid.elevation(idx));
    };
    const auto deviationSq = [&](CoarseGrid::Index idx) {
        const double d = grid.elevation(idx) - q.reference.at(grid.center(idx));
        return d * d;
    };

    g_[start] = 0.0F;
    heap_.push_back({static_cast<float>(heuristic(start)), start});
    while (!heap_.empty()) {
        std::pop_heap(heap_.begin(), heap_.end(), greater);
        const CoarseGrid::Index cur = heap_.back().idx;
        heap_.pop_back();
        if ((meta_[cur] & kClosed) != 0U) {
            continue;
        }
        meta_[cur] |= kClosed;
        if (cur == goal) {
            break;
        }
        const double g_cur = g_[cur];
        const double z_cur = grid.elevation(cur);
        const double dev_cur = use_deviation ? deviationSq(cur) : 0.0;
        for (std::size_t k = 0; k < kGridSteps.size(); ++k) {
            const auto nb_opt = grid.neighbor(cur, k);
            if (!nb_opt || (meta_[*nb_opt] & kClosed) != 0U) {
                continue;
            }
            const CoarseGrid::Index nb = *nb_opt;
            grid.ensure(nb);
            if (grid.blocked(nb)) {
                continue;
            }
            if (nb != goal && !q.band.contains(grid.elevation(nb))) {
                continue;
            }
            const bool diag = kGridSteps[k].di != 0 && kGridSteps[k].dj != 0;
            if (diag) {
                const CoarseGrid::Index c1 = grid.cornerCell(nb, cur);
                const CoarseGrid::Index c2 = grid.cornerCell(cur, nb);
                grid.ensure(c1);
                grid.ensure(c2);
                if (grid.blocked(c1) || grid.blocked(c2)) {
                    continue;
                }
            }
            const double d = diag ? diagonal : grid.cellSize();
            const double dev = use_deviation ? 0.5 * (deviationSq(nb) + dev_cur) : 0.0;
            const double ng = g_cur + q.cost.edge(d, grid.elevation(nb) - z_cur, dev) +
                              partial * 0.5 * (grid.hazardFraction(nb) + grid.hazardFraction(cur)) * d +
                              caution * 0.5 * (grid.cautionFraction(nb) + grid.cautionFraction(cur)) * d;
            if (ng < static_cast<double>(g_[nb])) {
                g_[nb] = static_cast<float>(ng);
                meta_[nb] = static_cast<std::uint8_t>(kHasParent | k);
                heap_.push_back({static_cast<float>(ng + heuristic(nb)), nb});
                std::push_heap(heap_.begin(), heap_.end(), greater);
            }
        }
    }
    if ((meta_[goal] & kClosed) == 0U) {
        return false;
    }
    out.clear();
    for (CoarseGrid::Index c = goal;;) {
        out.push_back(c);
        if ((meta_[c] & kHasParent) == 0U) {
            break;
        }
        // Step back: parent = c - step[k]  <=>  neighbour in the opposite direction (7 - k).
        c = *grid.neighbor(c, kGridSteps.size() - 1U - (meta_[c] & kDirMask));
    }
    std::reverse(out.begin(), out.end());
    return out.front() == start;
}

}  // namespace marspath::detail
