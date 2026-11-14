// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace marspath {

struct PlanStatistics {
    double length_m{0.0};
    double ascent_m{0.0};
    double descent_m{0.0};
    double max_slope{0.0};             ///< Between consecutive waypoints.
    double elevation_stddev_m{0.0};    ///< Distance-weighted.
    double elevation_range_m{0.0};
    double band_low_m{0.0};
    double band_high_m{0.0};
    double grid_cell_m{0.0};
    double final_ascent_weight{0.0};
    double final_descent_weight{0.0};
    double global_ms{0.0};
    double local_ms{0.0};
    std::uint64_t hazard_queries{0};
    std::uint64_t elevation_queries{0};
    std::uint32_t grid_cells_evaluated{0};
    std::uint8_t lagrange_iterations{0};
    std::uint8_t repairs{0};
    bool used_global_search{false};
};

}  // namespace marspath
