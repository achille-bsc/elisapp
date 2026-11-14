// SPDX-License-Identifier: MIT
#pragma once

#include "core/pathfinding/GeoPoint.hpp"

namespace marspath {

/// Output waypoint: position and terrain elevation (metres).
struct Waypoint {
    GeoPoint position{};
    float elevation_m{0.0F};
};

}  // namespace marspath
