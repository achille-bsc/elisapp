#pragma once

#include "../core/coord.hpp"
#include "../grid/grid.hpp"
#include "../radiation/radiation.hpp"
#include <cstdint>

// Hazard zones for a crew on foot, from every data of a point (terrain,
// local weather, radiation). Limits and their sources: config.hpp
// (NASA-STD-3001 Vol. 1 Rev B, xEVA concept of operations, Mars Exploration
// Zone). The level of a point is the worst level of its reasons.

// Enumerations //
enum e_hazard_level { HAZARD_SAFE, HAZARD_CAUTION, HAZARD_DANGER };

// Constants //
// Reasons, bit flags. Point reasons:
constexpr uint16_t HAZARD_NO_TERRAIN = 1u << 0;     // danger: unknown ground
constexpr uint16_t HAZARD_COARSE_TERRAIN = 1u << 1; // caution: MOLA slope only
constexpr uint16_t HAZARD_STEEP = 1u << 2;          // caution: > 15 deg
constexpr uint16_t HAZARD_TOO_STEEP = 1u << 3;      // danger: > 20 deg
constexpr uint16_t HAZARD_WIND = 1u << 4;           // danger
constexpr uint16_t HAZARD_DUST_STORM = 1u << 5;     // danger
constexpr uint16_t HAZARD_RADIATION_HIGH = 1u << 6; // caution: ALARA
constexpr uint16_t HAZARD_RADIATION_CRITICAL = 1u << 7; // danger: career limit
constexpr uint16_t HAZARD_SPE_ALERT = 1u << 8;      // danger: seek shelter
constexpr uint16_t HAZARD_NO_WEATHER = 1u << 9;     // information only
// Route reasons:
constexpr uint16_t HAZARD_EVA_TOO_LONG = 1u << 10;  // caution: > 8 h on foot
constexpr uint16_t HAZARD_BEYOND_ZONE = 1u << 11;   // caution: > 100 km away
constexpr uint16_t HAZARD_DOSE_BUDGET = 1u << 12;   // danger: career dose left

constexpr uint16_t HAZARD_DANGER_REASONS =
    HAZARD_NO_TERRAIN | HAZARD_TOO_STEEP | HAZARD_WIND | HAZARD_DUST_STORM |
    HAZARD_RADIATION_CRITICAL | HAZARD_SPE_ALERT | HAZARD_DOSE_BUDGET;
constexpr uint16_t HAZARD_CAUTION_REASONS =
    HAZARD_COARSE_TERRAIN | HAZARD_STEEP | HAZARD_RADIATION_HIGH |
    HAZARD_EVA_TOO_LONG | HAZARD_BEYOND_ZONE;

// Structures //
struct s_hazard {
  uint16_t reasons;         // HAZARD_* bits
  uint16_t dose_eq_usv_day; // ALARA: a cost for the pathfinding
  uint8_t level;            // e_hazard_level
} typedef t_hazard;

struct s_route_hazard {
  uint32_t length_mm;
  uint32_t duration_s;  // at the walking speed
  uint32_t dose_eq_usv; // received along the route
  uint16_t reasons;     // every point reason met, plus the route reasons
  uint8_t level;        // e_hazard_level, worst of all
} typedef t_route_hazard;

// Functions //
// Space weather warning of a solar particle event (NASA-STD-3001 V2 6100):
// while it is on, every point outside a shelter is a danger.
void hazard_set_spe_alert(bool active);
bool hazard_spe_alert();
uint8_t hazard_level_of(uint16_t reasons);
// Radiation part of the assessment: HAZARD_RADIATION_CRITICAL, _HIGH or 0
uint16_t hazard_radiation_reasons(const t_radiation &rad);
t_hazard hazard_assess(t_coord coord, uint32_t time_s);
// Hazard zones: one e_hazard_level per grid cell into `levels`.
// Returns the cells written, 0 if capacity < grid_cell_count().
uint32_t hazard_map(const t_grid *grid, uint32_t time_s, uint8_t *levels,
                    uint32_t capacity);
// Route walked point after point from `start_time_s`, each point assessed at
// its time of passage. speed_mm_s = 0: WALK_SPEED_MM_S.
t_route_hazard hazard_route(const t_coord *points, uint32_t count,
                            uint32_t start_time_s, uint32_t speed_mm_s);
