// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/detail/AltitudeBand.hpp"
#include "core/pathfinding/detail/CoarseGrid.hpp"

namespace marspath::detail {

/// Finds the narrowest altitude band [low, high] that still connects start and goal
/// (two-pointer sweep over breadth-first reachability tests).
/// Memory: 1 bit per cell + the BFS queue.
class BandSearch {
public:
    [[nodiscard]] std::optional<AltitudeBand> narrowest(CoarseGrid& grid, const ElevationParams& params);

private:
    [[nodiscard]] bool reachable(CoarseGrid& grid, double low, double high);
    [[nodiscard]] static bool passable(CoarseGrid& grid, CoarseGrid::Index idx, double low, double high);

    std::vector<std::uint64_t> seen_;
    std::vector<std::uint32_t> queue_;
};

}  // namespace marspath::detail
