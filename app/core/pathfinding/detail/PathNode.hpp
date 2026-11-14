// SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

/// Path vertex: 12 bytes. Local coordinates in float are precise to a few centimetres
/// over +-500 km, which is far below the data resolution.
struct PathNode {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};  ///< Elevation (m).

    [[nodiscard]] Vec2 pos() const noexcept { return {x, y}; }
    static PathNode at(Vec2 p, double elevation = 0.0) noexcept {
        return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(elevation)};
    }
};
using Path = std::vector<PathNode>;

}  // namespace marspath::detail
