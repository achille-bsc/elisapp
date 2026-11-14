// SPDX-License-Identifier: MIT
#pragma once

#include "core/pathfinding/GeoPoint.hpp"

namespace marspath {

/// Abstraction of the hazard map (Interface Segregation / Dependency Inversion):
/// the planner only needs to know whether a point is hazardous.
class IRiskMap {
public:
    IRiskMap() = default;
    IRiskMap(const IRiskMap&) = delete;
    IRiskMap& operator=(const IRiskMap&) = delete;
    IRiskMap(IRiskMap&&) = delete;
    IRiskMap& operator=(IRiskMap&&) = delete;
    virtual ~IRiskMap() = default;

    /// True if the point lies in a hazard (crater, crevasse, soft or slippery ground...).
    /// Called millions of times: implementations should be fast (spatial index, raster).
    [[nodiscard]] virtual bool isHazard(GeoPoint point) const = 0;
};

}  // namespace marspath
