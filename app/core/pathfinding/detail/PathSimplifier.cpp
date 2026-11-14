// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/PathSimplifier.hpp"

#include <algorithm>
#include <cmath>

#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

bool PathSimplifier::withinTolerance(const Path& path, std::size_t i, std::size_t j) const noexcept {
    const PathNode& u = path[i];
    const PathNode& v = path[j];
    const Vec2 uv = v.pos() - u.pos();
    const double l2 = dot(uv, uv);
    if (l2 < 1e-12) {
        return true;
    }
    for (std::size_t k = i + 1; k < j; ++k) {
        const PathNode& m = path[k];
        const double t = std::clamp(dot(m.pos() - u.pos(), uv) / l2, 0.0, 1.0);
        if (norm(m.pos() - (u.pos() + uv * t)) >= params_.simplify_tol_xy_m) {
            return false;
        }
        const double z_line = static_cast<double>(u.z) + static_cast<double>(v.z - u.z) * t;
        if (std::fabs(static_cast<double>(m.z) - z_line) >= params_.simplify_tol_z_m) {
            return false;
        }
    }
    return true;
}

void PathSimplifier::simplify(Path& path) const {
    constexpr std::size_t kMaxSpan = 200;
    const std::size_t n = path.size();
    if (n < 3) {
        return;
    }
    std::size_t w = 1;
    std::size_t i = 0;
    while (i + 1 < n) {
        std::size_t j = i + 1;
        while (j + 1 < n && j + 1 - i <= kMaxSpan && withinTolerance(path, i, j + 1)) {
            ++j;
        }
        while (j > i + 1 && !probe_.segmentClear(path[i].pos(), path[j].pos())) {
            --j;
        }
        path[w++] = path[j];
        i = j;
    }
    path.resize(w);
}

}  // namespace marspath::detail
