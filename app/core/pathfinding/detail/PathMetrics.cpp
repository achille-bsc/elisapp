// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/PathMetrics.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

void computeMetrics(const Path& path, PlanStatistics& s) noexcept {
    s.length_m = s.ascent_m = s.descent_m = s.max_slope = 0.0;
    s.elevation_stddev_m = s.elevation_range_m = 0.0;
    if (path.empty()) {
        return;
    }
    double z_min = std::numeric_limits<double>::infinity();
    double z_max = -z_min;
    double weighted_sum = 0.0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const double z = path[i].z;
        z_min = std::min(z_min, z);
        z_max = std::max(z_max, z);
        if (i + 1 < path.size()) {
            const double d = norm(path[i + 1].pos() - path[i].pos());
            const double dh = static_cast<double>(path[i + 1].z) - z;
            s.length_m += d;
            (dh > 0.0 ? s.ascent_m : s.descent_m) += std::fabs(dh);
            if (d > 1e-6) {
                s.max_slope = std::max(s.max_slope, std::fabs(dh) / d);
            }
            weighted_sum += d * 0.5 * (z + static_cast<double>(path[i + 1].z));
        }
    }
    s.elevation_range_m = z_max - z_min;
    if (s.length_m <= 0.0) {
        return;
    }
    const double mean = weighted_sum / s.length_m;
    double variance = 0.0;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const double zm = 0.5 * (static_cast<double>(path[i].z) + static_cast<double>(path[i + 1].z)) - mean;
        variance += norm(path[i + 1].pos() - path[i].pos()) * zm * zm;
    }
    s.elevation_stddev_m = std::sqrt(variance / s.length_m);
}

}  // namespace marspath::detail
