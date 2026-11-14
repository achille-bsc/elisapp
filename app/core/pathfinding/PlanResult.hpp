// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "core/pathfinding/PlanStatistics.hpp"
#include "core/pathfinding/Status.hpp"
#include "core/pathfinding/Warning.hpp"
#include "core/pathfinding/Waypoint.hpp"

namespace marspath {

struct PlanResult {
    Status status{Status::NoPathFound};
    Warning warnings{Warning::None};
    std::vector<Waypoint> path;
    PlanStatistics stats{};

    [[nodiscard]] bool ok() const noexcept { return status == Status::Ok; }
};

}  // namespace marspath
