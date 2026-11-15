// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

/// Edge cost = distance + weighted ascent/descent + excess-slope penalty
///           + optional quadratic deviation from the reference altitude.
class CostModel {
public:
    CostModel(const CostParams& params, double deviation_weight) noexcept
        : p_{params}, deviation_weight_{deviation_weight},
          ascent_weight_{params.ascent_weight}, descent_weight_{params.descent_weight} {}

    void setWeights(double ascent, double descent) noexcept {
        ascent_weight_ = ascent;
        descent_weight_ = descent;
    }
    [[nodiscard]] double ascentWeight() const noexcept { return ascent_weight_; }
    [[nodiscard]] double descentWeight() const noexcept { return descent_weight_; }
    [[nodiscard]] bool usesDeviation() const noexcept { return deviation_weight_ > 0.0; }

    /// d: horizontal length (m), dh: elevation change (m), deviation_sq: mean squared
    /// deviation from the reference altitude (m^2).
    [[nodiscard]] double edge(double d, double dh, double deviation_sq) const noexcept {
        double c = d + ascent_weight_ * std::max(0.0, dh) + descent_weight_ * std::max(0.0, -dh);
        const double excess = std::fabs(dh) / d - p_.max_slope;
        if (excess > 0.0) {
            c += p_.slope_penalty * excess * excess * d;
        }
        if (deviation_weight_ > 0.0) {
            c += deviation_weight_ * (deviation_sq / 1e4) * d;
        }
        return c;
    }

    /// Admissible A* heuristic: distance plus the unavoidable climb or descent.
    [[nodiscard]] double heuristic(double distance, double dz_to_goal) const noexcept {
        return distance + ascent_weight_ * std::max(0.0, dz_to_goal) +
               descent_weight_ * std::max(0.0, -dz_to_goal);
    }

    /// Penalty for leaving the altitude band by `excess_m` over a length d.
    [[nodiscard]] double bandPenalty(double excess_m, double d) const noexcept {
        const double e = excess_m / 100.0;
        return p_.slope_penalty * e * e * d;
    }

    /// Heading-change cost: 2(1 - cos(theta)) ~ theta^2, without atan2.
    [[nodiscard]] double turn(Vec2 v1, Vec2 v2) const noexcept {
        const double nn = norm(v1) * norm(v2);
        return nn < 1e-9 ? 0.0 : p_.turn_weight * 2.0 * (1.0 - dot(v1, v2) / nn);
    }

    [[nodiscard]] double partialBlockPenalty() const noexcept { return p_.partial_block_penalty; }
    [[nodiscard]] double cautionPenalty() const noexcept { return p_.caution_penalty; }

private:
    CostParams p_;
    double deviation_weight_;
    double ascent_weight_;
    double descent_weight_;
};

}  // namespace marspath::detail
