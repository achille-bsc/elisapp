// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

#include "core/pathfinding/GeoPoint.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

/// Local equirectangular projection centred on a reference point:
/// (degrees x 1e7) <-> metres. Accurate to ~0.1 % over a few hundred kilometres.
class LocalProjection {
public:
    LocalProjection(GeoPoint reference, double radius_m) noexcept
        : lat0_{reference.lat_e7}, lon0_{reference.lon_e7} {
        constexpr double kUnitToRad = 1e-7 * std::numbers::pi / 180.0;
        m_per_unit_y_ = kUnitToRad * radius_m;
        m_per_unit_x_ = m_per_unit_y_ * std::cos(static_cast<double>(reference.lat_e7) * kUnitToRad);
    }

    [[nodiscard]] Vec2 toLocal(GeoPoint g) const noexcept {
        return {static_cast<double>(std::int64_t{g.lon_e7} - lon0_) * m_per_unit_x_,
                static_cast<double>(std::int64_t{g.lat_e7} - lat0_) * m_per_unit_y_};
    }
    [[nodiscard]] GeoPoint toGeo(Vec2 v) const noexcept {
        return {static_cast<std::int32_t>(lat0_ + roundToInt(v.y / m_per_unit_y_)),
                static_cast<std::int32_t>(lon0_ + roundToInt(v.x / m_per_unit_x_))};
    }

private:
    // std::llround is slow on some ARM targets.
    [[nodiscard]] static std::int64_t roundToInt(double v) noexcept {
        return static_cast<std::int64_t>(std::floor(v + 0.5));
    }
    std::int64_t lat0_;
    std::int64_t lon0_;
    double m_per_unit_x_{0.0};
    double m_per_unit_y_{0.0};
};

}  // namespace marspath::detail
