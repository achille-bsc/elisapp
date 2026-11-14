// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "core/pathfinding/GeoPoint.hpp"
#include "core/pathfinding/ITerrain.hpp"

namespace marspath {

/// Adapts any callable `float(GeoPoint)` to ITerrain.
template <class F>
    requires std::is_invocable_r_v<float, const F&, GeoPoint>
class TerrainAdapter final : public ITerrain {
public:
    explicit TerrainAdapter(F function) noexcept(std::is_nothrow_move_constructible_v<F>)
        : function_(std::move(function)) {}
    [[nodiscard]] float elevation(GeoPoint point) const override { return std::invoke(function_, point); }

private:
    F function_;
};

}  // namespace marspath
