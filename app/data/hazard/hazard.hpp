#pragma once

#include "../core/coord.hpp"
#include "../grid/grid.hpp"
#include "../radiation/radiation.hpp"
#include <cstdint>

// Hazards for a crew on foot (terrain, ground, weather, radiation, storms).
// Limits and sources: config.hpp. A point's level is its worst reason's.

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
// Ground reasons (thermal inertia):
constexpr uint16_t HAZARD_SOFT_GROUND = 1u << 13;   // danger: loose fines
constexpr uint16_t HAZARD_DUSTY_GROUND = 1u << 14;  // caution: dusty

constexpr uint16_t HAZARD_DANGER_REASONS =
    HAZARD_NO_TERRAIN | HAZARD_TOO_STEEP | HAZARD_WIND | HAZARD_DUST_STORM |
    HAZARD_RADIATION_CRITICAL | HAZARD_SPE_ALERT | HAZARD_DOSE_BUDGET |
    HAZARD_SOFT_GROUND;
constexpr uint16_t HAZARD_CAUTION_REASONS =
    HAZARD_COARSE_TERRAIN | HAZARD_STEEP | HAZARD_RADIATION_HIGH |
    HAZARD_EVA_TOO_LONG | HAZARD_BEYOND_ZONE | HAZARD_DUSTY_GROUND;

// Forecast storms held at once (hazard_set_storms)
constexpr uint8_t HAZARD_MAX_STORMS = 8;
// Limits of a forecast storm: drift up to 100 m/s, a life of 30 sols
constexpr int32_t HAZARD_STORM_MAX_DRIFT_MM_S = 100000;
constexpr uint32_t HAZARD_STORM_MAX_SOLS = 30;
constexpr uint32_t HAZARD_STORM_CHECK_S = 600;

// Structures //
// Forecast dust storm: a disc drifting steadily between two mission times
struct s_storm {
  t_coord centre;          // at start_s
  int32_t drift_east_mm_s; // speed of the centre, + = east
  int32_t drift_north_mm_s;
  uint32_t radius_mm;
  uint32_t start_s;        // mission time
  uint32_t end_s;
  uint16_t wind_cms;       // strongest wind expected inside
  uint8_t dust_x40;        // dust opacity expected inside
  uint8_t reserved;
} typedef t_storm;

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
// Solar particle event warning (NASA-STD-3001 V2 6100): danger everywhere
void hazard_set_spe_alert(bool active);
bool hazard_spe_alert();
uint8_t hazard_level_of(uint16_t reasons);
// Radiation part of the assessment: HAZARD_RADIATION_CRITICAL, _HIGH or 0
uint16_t hazard_radiation_reasons(const t_radiation &rad);
t_hazard hazard_assess(t_coord coord, uint32_t time_s);
// hazard_assess() with the steepest terrain and softest ground within radius
t_hazard hazard_assess_area(t_coord coord, uint32_t time_s, uint32_t radius_mm);
// Danger here = dangerous ground within LANDFORM_CLEARANCE_M (50 m)
t_hazard hazard_assess_clearance(t_coord coord, uint32_t time_s);
// Ground part of the assessment: HAZARD_SOFT_GROUND, _DUSTY_GROUND or 0
uint16_t hazard_ground_reasons(t_coord coord);
// Replaces the forecast (copied). False and unchanged if a storm is invalid.
bool hazard_set_storms(const t_storm *storms, uint8_t count);
uint8_t hazard_storm_count();
const t_storm *hazard_storm(uint8_t index);
// Centre of a storm at a mission time (it drifts)
t_coord hazard_storm_centre(const t_storm &storm, uint32_t time_s);
// The storm over `coord` at `time_s`, nullptr if none
const t_storm *hazard_storm_at(t_coord coord, uint32_t time_s);
// A storm over `coord` at some time of [from_s, to_s]
const t_storm *hazard_storm_during(t_coord coord, uint32_t from_s, uint32_t to_s);
// The same with the radius widened by `margin_mm`
const t_storm *hazard_storm_near(t_coord coord, uint32_t from_s, uint32_t to_s,
                                 uint32_t margin_mm);
// Hazard zones: one e_hazard_level per grid cell into `levels`.
// Returns the cells written, 0 if capacity < grid_cell_count().
uint32_t hazard_map(const t_grid *grid, uint32_t time_s, uint8_t *levels,
                    uint32_t capacity);
// Route walked point after point from `start_time_s`, each point assessed at
// its time of passage. speed_mm_s = 0: WALK_SPEED_MM_S.
t_route_hazard hazard_route(const t_coord *points, uint32_t count,
                            uint32_t start_time_s, uint32_t speed_mm_s);
