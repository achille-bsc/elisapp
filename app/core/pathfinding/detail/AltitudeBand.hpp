// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace marspath::detail {

/// Allowed altitude band (inactive by default).
struct AltitudeBand {
    double low{-std::numeric_limits<double>::infinity()};
    double high{std::numeric_limits<double>::infinity()};

    [[nodiscard]] bool active() const noexcept { return std::isfinite(low) || std::isfinite(high); }
    [[nodiscard]] bool contains(double z) const noexcept { return z >= low && z <= high; }
    [[nodiscard]] double excess(double z) const noexcept { return std::max({0.0, z - high, low - z}); }
};

}  // namespace marspath::detail
