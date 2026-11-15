// SPDX-License-Identifier: MIT
#include "core/pathfinding/PlannerParams.hpp"

#include <cmath>

namespace marspath {

bool isValid(const PlannerParams& p) noexcept {
    const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
    const auto nonNegative = [](double v) { return std::isfinite(v) && v >= 0.0; };
    return positive(p.planet_radius_m) && nonNegative(p.safety.margin_m) && nonNegative(p.safety.check_step_m) &&
           nonNegative(p.elevation.max_ascent_m) && nonNegative(p.elevation.max_descent_m) &&
           p.elevation.band_slack >= 1.0 && nonNegative(p.elevation.band_extra_m) &&
           positive(p.elevation.band_step_m) && nonNegative(p.elevation.deviation_weight) &&
           positive(p.cost.ascent_weight) && nonNegative(p.cost.descent_weight) && positive(p.cost.max_slope) &&
           nonNegative(p.cost.slope_penalty) && nonNegative(p.cost.turn_weight) &&
           nonNegative(p.cost.partial_block_penalty) && nonNegative(p.cost.caution_penalty) && p.grid.max_side >= 8U && positive(p.grid.min_cell_m) &&
           positive(p.grid.corridor_ratio) && nonNegative(p.grid.corridor_min_m) &&
           nonNegative(p.local.sample_step_m) && positive(p.local.offset_step_m) &&
           p.local.max_offset_m >= p.local.offset_step_m && nonNegative(p.local.optim_step_m) &&
           positive(p.local.optim_min_step_m) && nonNegative(p.local.optim_min_gain) &&
           positive(p.local.simplify_tol_xy_m) && positive(p.local.simplify_tol_z_m) &&
           nonNegative(p.time_budget_ms);
}

}  // namespace marspath
