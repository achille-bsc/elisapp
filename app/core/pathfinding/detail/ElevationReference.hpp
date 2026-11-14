// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

/// Reference altitude: linear between start and goal altitudes along the route axis.
class ElevationReference {
public:
    ElevationReference(Vec2 start, Vec2 goal, double z_start, double z_goal) noexcept
        : start_{start}, axis_{normalized(goal - start)}, length_{std::max(norm(goal - start), 1e-9)},
          z_start_{z_start}, z_goal_{z_goal} {}

    [[nodiscard]] double at(Vec2 p) const noexcept {
        const double t = std::clamp(dot(p - start_, axis_) / length_, 0.0, 1.0);
        return z_start_ + (z_goal_ - z_start_) * t;
    }
    [[nodiscard]] double zStart() const noexcept { return z_start_; }
    [[nodiscard]] double zGoal() const noexcept { return z_goal_; }

private:
    Vec2 start_;
    Vec2 axis_;
    double length_;
    double z_start_;
    double z_goal_;
};

}  // namespace marspath::detail
