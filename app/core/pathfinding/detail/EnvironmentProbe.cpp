// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/EnvironmentProbe.hpp"

#include <cmath>
#include <cstddef>
#include <numbers>

namespace marspath::detail {

EnvironmentProbe::EnvironmentProbe(const IRiskMap& risk, const ITerrain& terrain,
                                   const LocalProjection& projection, Config config, Vec2 start,
                                   Vec2 goal) noexcept
    : risk_{risk},
      terrain_{terrain},
      projection_{projection},
      config_{config},
      start_{start},
      goal_{goal},
      ring_radius_m_{config.margin_m + 0.5 * config.check_step_m} {
    for (std::size_t k = 0; k < ring_.size(); ++k) {
        const double a = static_cast<double>(k) * std::numbers::pi / 6.0;
        ring_[k] = Vec2{std::cos(a), std::sin(a)} * ring_radius_m_;
    }
}

bool EnvironmentProbe::hazard(Vec2 p) const {
    ++hazard_queries_;
    return risk_.isHazard(projection_.toGeo(p));
}

double EnvironmentProbe::elevation(Vec2 p) const {
    ++elevation_queries_;
    return static_cast<double>(terrain_.elevation(projection_.toGeo(p)));
}

bool EnvironmentProbe::isClear(Vec2 p) const {
    if (hazard(p)) {
        return false;
    }
    if (config_.margin_m <= 0.0) {
        return true;
    }
    // No margin required right next to the start and the goal.
    if (norm(p - start_) < ring_radius_m_ || norm(p - goal_) < ring_radius_m_) {
        return true;
    }
    for (const Vec2& d : ring_) {
        if (hazard(p + d)) {
            return false;
        }
    }
    return true;
}

bool EnvironmentProbe::segmentClear(Vec2 a, Vec2 b) const {
    const int n = static_cast<int>(std::ceil(norm(b - a) / config_.check_step_m));
    for (int k = 1; k < n; ++k) {
        if (!isClear(a + (b - a) * (static_cast<double>(k) / n))) {
            return false;
        }
    }
    return true;
}

std::optional<Vec2> EnvironmentProbe::blockedChordMiddle(Vec2 a, Vec2 b) const {
    const int n = static_cast<int>(std::ceil(norm(b - a) / config_.check_step_m));
    int first = -1;
    int last = -1;
    for (int k = 1; k < n; ++k) {
        if (!isClear(a + (b - a) * (static_cast<double>(k) / n))) {
            if (first < 0) {
                first = k;
            }
            last = k;
        } else if (first >= 0) {
            break;
        }
    }
    if (first < 0) {
        return std::nullopt;
    }
    return a + (b - a) * (0.5 * (first + last) / n);
}

std::optional<Vec2> EnvironmentProbe::pushAlong(Vec2 p, Vec2 dir, double* distance) const {
    double lo = 0.0;
    double hi = -1.0;
    for (double d = config_.offset_step_m; d <= config_.max_offset_m; d += config_.offset_step_m) {
        if (isClear(p + dir * d)) {
            hi = d;
            break;
        }
        lo = d;
    }
    if (hi < 0.0) {
        return std::nullopt;
    }
    for (int k = 0; k < 3; ++k) {
        const double mid = 0.5 * (lo + hi);
        if (isClear(p + dir * mid)) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    if (distance != nullptr) {
        *distance = hi;
    }
    return p + dir * hi;
}

std::optional<Vec2> EnvironmentProbe::escape(Vec2 p, Vec2 preferred) const {
    double d1 = 0.0;
    double d2 = 0.0;
    const auto q1 = pushAlong(p, preferred, &d1);
    const auto q2 = pushAlong(p, preferred * -1.0, &d2);
    if (q1 && q2) {
        return d1 <= 1.5 * d2 ? q1 : q2;
    }
    if (q1) {
        return q1;
    }
    if (q2) {
        return q2;
    }
    return nearestClear(p);
}

std::optional<Vec2> EnvironmentProbe::nearestClear(Vec2 p) const {
    for (double r = config_.offset_step_m; r <= config_.max_offset_m; r += config_.offset_step_m) {
        for (int k = 0; k < 16; ++k) {
            const double a = k * std::numbers::pi / 8.0;
            const Vec2 q = p + Vec2{std::cos(a), std::sin(a)} * r;
            if (isClear(q)) {
                return q;
            }
        }
    }
    return std::nullopt;
}

}  // namespace marspath::detail
