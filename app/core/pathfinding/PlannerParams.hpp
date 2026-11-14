// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace marspath {

/// Clearance around the path.
struct SafetyParams {
    double margin_m{50.0};     ///< No hazard closer than this to the path.
    double check_step_m{0.0};  ///< Sampling step along segments (0 = margin / 2).
};

/// Elevation constraints and the "flat route" objective.
struct ElevationParams {
    double max_ascent_m{500.0};   ///< Cumulative ascent limit (0 = none).
    double max_descent_m{500.0};  ///< Cumulative descent limit (0 = none).
    bool altitude_band{true};     ///< Restrict the route to the narrowest feasible altitude band.
    double band_slack{1.2};       ///< Band widening factor.
    double band_extra_m{50.0};    ///< Band widening margin.
    double band_step_m{50.0};     ///< Band search resolution.
    double deviation_weight{0.0}; ///< Quadratic penalty on deviation from the reference altitude.
};

/// Cost model, in "equivalent metres".
struct CostParams {
    double ascent_weight{8.0};          ///< Initial cost of 1 m of ascent (raised if needed).
    double descent_weight{2.0};         ///< Initial cost of 1 m of descent (raised if needed).
    double max_slope{0.15};             ///< Slope above which a strong penalty applies.
    double slope_penalty{2000.0};       ///< Weight of the excess-slope penalty.
    double turn_weight{20.0};           ///< Weight of heading changes (local stage).
    double partial_block_penalty{3.0};  ///< Extra cost of partly hazardous coarse cells.
};

/// Global coarse search (A* on a lazily evaluated grid aligned with the route).
struct GridParams {
    std::uint32_t max_side{512};             ///< Max number of cells along the route.
    double min_cell_m{100.0};
    double corridor_ratio{0.25};             ///< Corridor half-width as a fraction of the distance.
    double corridor_min_m{3000.0};
    std::uint8_t max_lagrange_iterations{8}; ///< A* restarts to meet the elevation limits.
    std::uint8_t max_repairs{5};             ///< A* restarts after a local blockage.
};

/// Local refinement and optimisation.
struct LocalParams {
    double sample_step_m{0.0};      ///< Point spacing (0 = automatic).
    double offset_step_m{10.0};     ///< Step used to push points out of hazards.
    double max_offset_m{5000.0};
    std::uint32_t max_insertions{5000};
    double optim_step_m{0.0};       ///< Initial displacement (0 = automatic).
    double optim_min_step_m{10.0};
    std::uint16_t optim_max_passes{5};
    double optim_min_gain{2.0};
    std::uint16_t optim_window{2};  ///< Half-width (points) of sections moved as a block.
    bool simplify{true};
    double simplify_tol_xy_m{2.0};
    double simplify_tol_z_m{1.0};
};

struct PlannerParams {
    double planet_radius_m{3389500.0};  ///< Mars by default (Earth: 6371000).
    SafetyParams safety{};
    ElevationParams elevation{};
    CostParams cost{};
    GridParams grid{};
    LocalParams local{};
    double time_budget_ms{0.0};  ///< > 0: local optimisation stops at this deadline.
};

/// True if every parameter is within its valid range.
[[nodiscard]] bool isValid(const PlannerParams& params) noexcept;

}  // namespace marspath
