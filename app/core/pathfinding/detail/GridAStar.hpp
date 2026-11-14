// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "core/pathfinding/detail/AltitudeBand.hpp"
#include "core/pathfinding/detail/CoarseGrid.hpp"
#include "core/pathfinding/detail/CostModel.hpp"
#include "core/pathfinding/detail/ElevationReference.hpp"

namespace marspath::detail {

/// A* on the coarse grid. Memory: 5 bytes per cell (float g + packed parent/closed).
class GridAStar {
public:
    struct Query {
        const CostModel& cost;
        const ElevationReference& reference;
        const AltitudeBand& band;
    };
    /// Fills `out` with the cell sequence start -> goal. False if no path exists.
    [[nodiscard]] bool run(CoarseGrid& grid, const Query& query, std::vector<CoarseGrid::Index>& out);

private:
    struct HeapEntry {
        float f;
        CoarseGrid::Index idx;
    };
    static constexpr std::uint8_t kClosed = 0x80U;
    static constexpr std::uint8_t kHasParent = 0x40U;
    static constexpr std::uint8_t kDirMask = 0x07U;

    std::vector<float> g_;
    std::vector<std::uint8_t> meta_;
    std::vector<HeapEntry> heap_;
};

}  // namespace marspath::detail
