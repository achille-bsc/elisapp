#pragma once

#include "../core/coord.hpp"
#include "../landform/landform.hpp"
#include "../link/weather_protocol.hpp"
#include <cstdint>

// Surface radiation estimate (galactic cosmic rays), from the shielding of
// the atmosphere above the point. Curiosity's RAD showed that the dose rate
// follows the column mass, so the surface pressure:
//   D = D01 + KAPPA * (P - P0) + BETA * PHI        (Guo et al. 2015, Eq. 7)
// P comes from weather_model (MOLA elevation + diurnal cycle), PHI (solar
// modulation) from config.hpp.
// The fit was made at Gale (~700-950 Pa): far from it (high volcanoes) it is
// a rough extrapolation. Solar particle events are not modelled.

// Constants //
constexpr int32_t NGY_PER_UGY = 1000;
constexpr uint32_t PERCENT = 100;

// Structures //
struct s_radiation {
  uint16_t dose_ugy_day;    // absorbed dose rate, micrograys per sol
  uint16_t dose_eq_usv_day; // dose equivalent rate, microsieverts per sol
  bool critical;            // dose_eq >= RADIATION_CRITICAL_USV_DAY
  bool valid;
} typedef t_radiation;

// Functions //
// The model alone: local pressure + solar modulation -> absorbed dose
uint16_t radiation_dose_ugy_day(uint16_t pressure_dpa, int32_t solar_mv);
// From a local pressure (dose, dose equivalent, critical flag)
t_radiation radiation_at_pressure(uint16_t pressure_dpa);
// From data already read at a point: the local weather if valid, else the
// standard datum pressure corrected by the elevation. Invalid without ground.
t_radiation radiation_from(const t_landform &land, const t_weather &local);
// At a point and a time. Invalid outside the landform zone.
t_radiation radiation_estimate(t_coord coord, uint32_t time_s);
