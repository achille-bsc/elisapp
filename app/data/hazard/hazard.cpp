#include "./hazard.hpp"
#include "../config.hpp"
#include "../core/geodesy.hpp"
#include "../landform/landform.hpp"
#include "../radiation/radiation.hpp"
#include "../weather/weather_model.hpp"

static bool g_spe_alert = false;

// ALARA: caution from RAD_CAUTION_PERCENT of the critical rate
static constexpr uint32_t RADIATION_CAUTION_USV_DAY =
    RADIATION_CRITICAL_USV_DAY * RAD_CAUTION_PERCENT / PERCENT;
// Dose a crew member may still receive before the career limit
static constexpr uint32_t RAD_BUDGET_LEFT_USV =
    RAD_CAREER_LIMIT_USV - RAD_CREW_DOSE_RECEIVED_USV;

void hazard_set_spe_alert(bool active) { g_spe_alert = active; }

bool hazard_spe_alert() { return (g_spe_alert); }

uint8_t hazard_level_of(uint16_t reasons) {
  if (reasons & HAZARD_DANGER_REASONS)
    return (HAZARD_DANGER);
  return (reasons & HAZARD_CAUTION_REASONS ? HAZARD_CAUTION : HAZARD_SAFE);
}

static uint16_t terrain_reasons(const t_landform &land) {
  if (!land.valid)
    return (HAZARD_NO_TERRAIN);
  uint16_t reasons = 0;
  if (land.slope_half_deg > HAZARD_SLOPE_DANGER_HALF_DEG)
    reasons |= HAZARD_TOO_STEEP;
  else if (land.slope_half_deg > HAZARD_SLOPE_CAUTION_HALF_DEG)
    reasons |= HAZARD_STEEP;
  const t_landform_header *layer = landform_layer(land.layer);
  if (layer && layer->step_udeg > HAZARD_FINE_TERRAIN_MAX_STEP_UDEG)
    reasons |= HAZARD_COARSE_TERRAIN;
  return (reasons);
}

static uint16_t weather_reasons(const t_weather &wx) {
  if (!wx.valid)
    return (HAZARD_NO_WEATHER);
  uint16_t reasons = 0;
  if (wx.wind_cms > HAZARD_WIND_CMS)
    reasons |= HAZARD_WIND;
  if (wx.dust_x40 > HAZARD_DUST_X40)
    reasons |= HAZARD_DUST_STORM;
  return (reasons);
}

uint16_t hazard_radiation_reasons(const t_radiation &rad) {
  if (!rad.valid)
    return (0);
  if (rad.critical)
    return (HAZARD_RADIATION_CRITICAL);
  return (rad.dose_eq_usv_day > RADIATION_CAUTION_USV_DAY ? HAZARD_RADIATION_HIGH
                                                          : 0);
}

t_hazard hazard_assess(t_coord coord, uint32_t time_s) {
  const t_landform land = landform_get(coord);
  const t_weather wx = weather_estimate(coord, time_s);
  const t_radiation rad = radiation_from(land, wx);
  t_hazard h = {};
  h.reasons = static_cast<uint16_t>(terrain_reasons(land) | weather_reasons(wx) |
                                    hazard_radiation_reasons(rad) |
                                    (g_spe_alert ? HAZARD_SPE_ALERT : 0));
  h.dose_eq_usv_day = rad.dose_eq_usv_day;
  h.level = hazard_level_of(h.reasons);
  return (h);
}

uint32_t hazard_map(const t_grid *grid, uint32_t time_s, uint8_t *levels,
                    uint32_t capacity) {
  const uint32_t count = grid_cell_count(grid);
  if (!levels || capacity < count)
    return (0);
  for (uint32_t i = 0; i < count; i++)
    levels[i] = hazard_assess(grid_cell_coord(grid, i), time_s).level;
  return (count);
}

static uint32_t saturate_u32(uint64_t v) {
  return (static_cast<uint32_t>(v > UINT32_MAX ? UINT32_MAX : v));
}

t_route_hazard hazard_route(const t_coord *points, uint32_t count,
                            uint32_t start_time_s, uint32_t speed_mm_s) {
  t_route_hazard route = {};
  if (!points || count == 0)
    return (route);
  const uint32_t speed = speed_mm_s ? speed_mm_s : WALK_SPEED_MM_S;
  uint64_t length = 0;
  uint64_t elapsed_s = 0;
  // Dose rate (uSv/sol) times seconds: divided once at the end, no rounding
  uint64_t dose_usv_s = 0;
  for (uint32_t i = 0; i < count; i++) {
    const t_hazard h =
        hazard_assess(points[i], saturate_u32(start_time_s + elapsed_s));
    route.reasons |= h.reasons;
    if (coord_distance_mm(points[0], points[i]) > EXPLORATION_ZONE_RADIUS_MM)
      route.reasons |= HAZARD_BEYOND_ZONE;
    if (i + 1 == count)
      break;
    // Segment i -> i + 1 walked at the dose rate of its start
    const uint32_t step_mm = coord_distance_mm(points[i], points[i + 1]);
    const uint64_t step_s = step_mm / speed;
    length += step_mm;
    elapsed_s += step_s;
    dose_usv_s += uint64_t{h.dose_eq_usv_day} * step_s;
  }
  route.length_mm = saturate_u32(length);
  route.duration_s = saturate_u32(elapsed_s);
  route.dose_eq_usv = saturate_u32(dose_usv_s / MARS_SOL_S);
  if (route.duration_s > EVA_MAX_DURATION_S)
    route.reasons |= HAZARD_EVA_TOO_LONG;
  if (route.dose_eq_usv > RAD_BUDGET_LEFT_USV)
    route.reasons |= HAZARD_DOSE_BUDGET;
  route.level = hazard_level_of(route.reasons);
  return (route);
}

static_assert(sizeof(t_hazard) == 6);
static_assert(sizeof(t_route_hazard) == 16);
static_assert((HAZARD_DANGER_REASONS & HAZARD_CAUTION_REASONS) == 0,
              "a reason has one level");
static_assert(RADIATION_CAUTION_USV_DAY < RADIATION_CRITICAL_USV_DAY);
static_assert(HAZARD_SLOPE_DANGER_HALF_DEG <= LANDFORM_MAX_SLOPE_HALF_DEG);
// The MOLA layer must count as coarse, a 20 m CTX layer as fine
static_assert(LANDFORM_MOLA_STEP_UDEG > HAZARD_FINE_TERRAIN_MAX_STEP_UDEG);
// u64 dose accumulator: rate <= u16, seconds <= 136 years in u32 per point
static_assert(uint64_t{UINT16_MAX} * UINT32_MAX < UINT64_MAX / UINT16_MAX);
