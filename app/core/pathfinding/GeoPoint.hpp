// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace marspath {

/// Geographic coordinate in degrees x 1e7 (MAVLink / u-blox convention).
struct GeoPoint {
    std::int32_t lat_e7{0};
    std::int32_t lon_e7{0};

    friend constexpr bool operator==(const GeoPoint&, const GeoPoint&) noexcept = default;
};

}  // namespace marspath
