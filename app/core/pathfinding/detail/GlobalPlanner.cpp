// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/GlobalPlanner.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace marspath::detail {

GlobalPlanner::Outcome GlobalPlanner::plan(const ElevationParams& elevation, const GridParams& grid_params,
                                           const ElevationReference& reference, CostModel& cost,
                                           AltitudeBand& band, Vec2 start, Vec2 goal, Path& path) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    Outcome outcome;

    band = AltitudeBand{};
    if (elevation.altitude_band) {
        const auto narrowest = band_search_.narrowest(grid_, elevation);
        if (!narrowest) {
            return outcome;
        }
        band = *narrowest;
    }

    // Coarse targets are 15 % below the limits: fine relief adds elevation change
    // that the grid cannot see.
    const double target_up = elevation.max_ascent_m > 0.0 ? 0.85 * elevation.max_ascent_m : kInf;
    const double target_down = elevation.max_descent_m > 0.0 ? 0.85 * elevation.max_descent_m : kInf;
    double w_up = cost.ascentWeight();
    double w_down = cost.descentWeight();
    double best_score = kInf;
    double best_w_up = w_up;
    double best_w_down = w_down;
    double previous_score = kInf;
    bool met = false;
    best_.clear();

    const GridAStar::Query query{cost, reference, band};
    for (std::uint8_t it = 0; it < std::max<std::uint8_t>(1U, grid_params.max_lagrange_iterations); ++it) {
        ++outcome.iterations;
        cost.setWeights(w_up, w_down);
        if (!astar_.run(grid_, query, cells_)) {
            break;
        }
        double up = 0.0;
        double down = 0.0;
        for (std::size_t k = 1; k < cells_.size(); ++k) {
            const double dz = grid_.elevation(cells_[k]) - grid_.elevation(cells_[k - 1]);
            (dz > 0.0 ? up : down) += std::fabs(dz);
        }
        const double score = std::max(0.0, up - target_up) + std::max(0.0, down - target_down);
        if (score < best_score) {
            best_.swap(cells_);
            best_score = score;
            best_w_up = w_up;
            best_w_down = w_down;
        }
        if (score <= 0.0) {
            met = true;
            break;
        }
        if (score > 0.9 * previous_score) {
            break;  // a restart barely helped: the limits are out of reach
        }
        previous_score = score;
        if (up > target_up) {
            w_up *= 3.0;
        }
        if (down > target_down) {
            w_down *= 3.0;
        }
    }
    if (best_.empty()) {
        return outcome;
    }
    cost.setWeights(best_w_up, best_w_down);
    if (!met) {
        outcome.warnings |= Warning::ConstraintUnreachable;
        const double net = grid_.elevation(grid_.goalCell()) - grid_.elevation(grid_.startCell());
        if ((elevation.max_ascent_m > 0.0 && net > elevation.max_ascent_m) ||
            (elevation.max_descent_m > 0.0 && -net > elevation.max_descent_m)) {
            outcome.warnings |= Warning::NetElevationExceedsLimit;
        }
    }

    // Coarse route -> polyline (only direction changes are kept).
    path.clear();
    path.push_back(PathNode::at(start));
    for (std::size_t k = 1; k + 1 < best_.size(); ++k) {
        const auto d_prev = static_cast<std::int64_t>(best_[k]) - best_[k - 1];
        const auto d_next = static_cast<std::int64_t>(best_[k + 1]) - best_[k];
        if (d_prev != d_next) {
            path.push_back(PathNode::at(grid_.center(best_[k])));
        }
    }
    path.push_back(PathNode::at(goal));
    outcome.found = true;
    return outcome;
}

}  // namespace marspath::detail
