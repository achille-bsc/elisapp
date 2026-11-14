// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/Warning.hpp"
#include "core/pathfinding/detail/AltitudeBand.hpp"
#include "core/pathfinding/detail/BandSearch.hpp"
#include "core/pathfinding/detail/CoarseGrid.hpp"
#include "core/pathfinding/detail/CostModel.hpp"
#include "core/pathfinding/detail/ElevationReference.hpp"
#include "core/pathfinding/detail/GridAStar.hpp"
#include "core/pathfinding/detail/PathNode.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

// Internal: coarse route on the grid, honouring the elevation limits by Lagrangian
// relaxation (the ascent / descent weights are raised until the limits are met).

class GlobalPlanner {
public:
    struct Outcome {
        bool found{false};
        Warning warnings{Warning::None};
        std::uint8_t iterations{0};
    };

    GlobalPlanner(CoarseGrid& grid, GridAStar& astar, BandSearch& band_search) noexcept
        : grid_{grid}, astar_{astar}, band_search_{band_search} {}

    /// Computes the altitude band (if enabled) and the coarse route. On success, `path`
    /// holds start, the coarse corners and goal; `cost` holds the final weights.
    [[nodiscard]] Outcome plan(const ElevationParams& elevation, const GridParams& grid_params,
                               const ElevationReference& reference, CostModel& cost, AltitudeBand& band,
                               Vec2 start, Vec2 goal, Path& path);

private:
    CoarseGrid& grid_;
    GridAStar& astar_;
    BandSearch& band_search_;
    std::vector<CoarseGrid::Index> cells_;
    std::vector<CoarseGrid::Index> best_;
};

}  // namespace marspath::detail
