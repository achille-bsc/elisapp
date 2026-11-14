// SPDX-License-Identifier: MIT
#pragma once

#include "core/pathfinding/PlanStatistics.hpp"
#include "core/pathfinding/detail/PathNode.hpp"

namespace marspath::detail {

/// Length, ascent/descent, max slope, distance-weighted elevation standard deviation and range.
void computeMetrics(const Path& path, PlanStatistics& stats) noexcept;

}  // namespace marspath::detail
