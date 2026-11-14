// SPDX-License-Identifier: MIT
#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include "core/pathfinding/GeoPoint.hpp"
#include "core/pathfinding/IRiskMap.hpp"

namespace marspath {

/// Adapts any callable `bool(GeoPoint)` to IRiskMap.
template <class F>
    requires std::predicate<const F&, GeoPoint>
class RiskMapAdapter final : public IRiskMap {
public:
    explicit RiskMapAdapter(F function) noexcept(std::is_nothrow_move_constructible_v<F>)
        : function_(std::move(function)) {}
    [[nodiscard]] bool isHazard(GeoPoint point) const override { return std::invoke(function_, point); }

private:
    F function_;
};

}  // namespace marspath
