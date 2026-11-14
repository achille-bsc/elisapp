// SPDX-License-Identifier: MIT
#pragma once

#include "core/pathfinding/GeoPoint.hpp"

namespace marspath {

/// Abstraction of the terrain model.
class ITerrain {
public:
    ITerrain() = default;
    ITerrain(const ITerrain&) = delete;
    ITerrain& operator=(const ITerrain&) = delete;
    ITerrain(ITerrain&&) = delete;
    ITerrain& operator=(ITerrain&&) = delete;
    virtual ~ITerrain() = default;

    /// Terrain elevation in metres at the given point.
    [[nodiscard]] virtual float elevation(GeoPoint point) const = 0;
};

}  // namespace marspath
