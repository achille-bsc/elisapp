#include "./hazard.hpp"
#include "../config.hpp"
#include "../core/geodesy.hpp"
#include "../ground/ground.hpp"
#include "../landform/landform.hpp"
#include "../radiation/radiation.hpp"
#include "../weather/weather_model.hpp"

static bool g_spe_alert = false;
static t_storm g_storms[HAZARD_MAX_STORMS];
static uint8_t g_storm_count = 0;

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

// hazard_storm_during() skips a storm farther than its reach + 1/16 of it
static constexpr uint64_t STORM_REACH_MARGIN_DIV = 16;

static bool storm_is_valid(const t_storm &s) {
  const auto within = [](int32_t v) {
    return (v >= -HAZARD_STORM_MAX_DRIFT_MM_S && v <= HAZARD_STORM_MAX_DRIFT_MM_S);
  };
  return (coord_is_valid(s.centre) && within(s.drift_east_mm_s) &&
          within(s.drift_north_mm_s) && s.end_s > s.start_s &&
          s.end_s - s.start_s <= HAZARD_STORM_MAX_SOLS * MARS_SOL_S);
}

bool hazard_set_storms(const t_storm *storms, uint8_t count) {
  if (count > HAZARD_MAX_STORMS || (count && !storms))
    return (false);
  for (uint8_t i = 0; i < count; i++)
    if (!storm_is_valid(storms[i]))
      return (false);
  for (uint8_t i = 0; i < count; i++)
    g_storms[i] = storms[i];
  g_storm_count = count;
  return (true);
}

uint8_t hazard_storm_count() { return (g_storm_count); }

const t_storm *hazard_storm(uint8_t index) {
  return (index < g_storm_count ? &g_storms[index] : nullptr);
}

t_coord hazard_storm_centre(const t_storm &storm, uint32_t time_s) {
  // Before its start it stands at its first place, after its end at its last
  int64_t dt = time_s > storm.start_s ? int64_t{time_s} - storm.start_s : 0;
  if (dt > int64_t{storm.end_s} - storm.start_s)
    dt = int64_t{storm.end_s} - storm.start_s;
  const int64_t north_mm = int64_t{storm.drift_north_mm_s} * dt;
  const int64_t east_mm = int64_t{storm.drift_east_mm_s} * dt;
  int64_t lat = storm.centre.lat_udeg +
                north_mm * Q16_ONE / int64_t{MM_PER_UDEG_LAT_Q16};
  if (lat > LAT_MAX_UDEG)
    lat = LAT_MAX_UDEG;
  if (lat < -LAT_MAX_UDEG)
    lat = -LAT_MAX_UDEG;
  const int64_t lon_mm_q16 = mm_per_udeg_lon_q16(static_cast<int32_t>(lat));
  const int64_t lon = int64_t{storm.centre.lon_udeg} +
                      (lon_mm_q16 ? east_mm * Q16_ONE / lon_mm_q16 : 0);
  return (t_coord{lon_wrap_udeg(lon), static_cast<int32_t>(lat)});
}

const t_storm *hazard_storm_at(t_coord coord, uint32_t time_s) {
  for (uint8_t i = 0; i < g_storm_count; i++) {
    const t_storm &s = g_storms[i];
    if (time_s >= s.start_s && time_s < s.end_s &&
        coord_distance_mm(hazard_storm_centre(s, time_s), coord) <= s.radius_mm)
      return (&s);
  }
  return (nullptr);
}

const t_storm *hazard_storm_near(t_coord coord, uint32_t from_s, uint32_t to_s,
                                 uint32_t margin_mm) {
  for (uint8_t i = 0; i < g_storm_count; i++) {
    const t_storm &s = g_storms[i];
    const uint32_t start = from_s > s.start_s ? from_s : s.start_s;
    const uint32_t end = to_s < s.end_s ? to_s : s.end_s - 1;
    if (start > end)
      continue;
    // The planner asks millions of points: skip a storm that cannot drift
    // that far within the window (|east| + |north| >= its speed, + a margin
    // for the flat-map distances)
    const uint64_t drift =
        uint64_t{static_cast<uint32_t>(s.drift_east_mm_s < 0 ? -s.drift_east_mm_s
                                                             : s.drift_east_mm_s)} +
        static_cast<uint32_t>(s.drift_north_mm_s < 0 ? -s.drift_north_mm_s
                                                     : s.drift_north_mm_s);
    const uint64_t radius = uint64_t{s.radius_mm} + margin_mm;
    const uint64_t reach = radius + drift * (end - start);
    if (coord_distance_mm(hazard_storm_centre(s, start), coord) >
        reach + reach / STORM_REACH_MARGIN_DIV)
      continue;
    for (uint64_t t = start; t <= end; t += HAZARD_STORM_CHECK_S) {
      const uint32_t at = static_cast<uint32_t>(t);
      if (coord_distance_mm(hazard_storm_centre(s, at), coord) <= radius)
        return (&s);
    }
    // The last moment of the window, between two checks
    if (coord_distance_mm(hazard_storm_centre(s, end), coord) <= radius)
      return (&s);
  }
  return (nullptr);
}

const t_storm *hazard_storm_during(t_coord coord, uint32_t from_s, uint32_t to_s) {
  return (hazard_storm_near(coord, from_s, to_s, 0));
}

static uint16_t ground_reasons(const t_ground &ground) {
  if (!ground.valid)
    return (0);
  if (ground.inertia_tiu < GROUND_SOFT_MAX_TIU)
    return (HAZARD_SOFT_GROUND);
  return (ground.inertia_tiu < GROUND_DUSTY_MAX_TIU ? HAZARD_DUSTY_GROUND : 0);
}

uint16_t hazard_ground_reasons(t_coord coord) {
  return (ground_reasons(ground_get(coord)));
}

static t_hazard assess(const t_landform &land, uint16_t ground, t_coord coord,
                       uint32_t time_s) {
  const t_weather wx = weather_estimate(coord, time_s);
  const t_radiation rad = radiation_from(landform_get(coord), wx);
  t_hazard h = {};
  uint16_t storm = 0;
  if (const t_storm *s = hazard_storm_at(coord, time_s))
    // A forecast storm is a danger whatever its strength; strong wind too
    storm = static_cast<uint16_t>(
        HAZARD_DUST_STORM | (s->wind_cms > HAZARD_WIND_CMS ? HAZARD_WIND : 0));
  h.reasons = static_cast<uint16_t>(terrain_reasons(land) | weather_reasons(wx) |
                                    hazard_radiation_reasons(rad) | storm |
                                    ground |
                                    (g_spe_alert ? HAZARD_SPE_ALERT : 0));
  h.dose_eq_usv_day = rad.dose_eq_usv_day;
  h.level = hazard_level_of(h.reasons);
  return (h);
}

t_hazard hazard_assess(t_coord coord, uint32_t time_s) {
  return (assess(landform_get(coord), hazard_ground_reasons(coord), coord, time_s));
}

t_hazard hazard_assess_clearance(t_coord coord, uint32_t time_s) {
  t_landform land = landform_get(coord);
  if (land.near_half_deg > land.slope_half_deg)
    land.slope_half_deg = land.near_half_deg;
  const uint32_t clearance_mm = LANDFORM_CLEARANCE_M * MM_PER_M_I;
  return (assess(land, ground_reasons(ground_softest(coord, clearance_mm)),
                 coord, time_s));
}

t_hazard hazard_assess_area(t_coord coord, uint32_t time_s, uint32_t radius_mm) {
  return (assess(landform_steepest(coord, radius_mm),
                 ground_reasons(ground_softest(coord, radius_mm)), coord, time_s));
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
static_assert(sizeof(t_storm) == 32, "no hidden padding");
// Storm drift: mm/s times a storm's life, times Q16, stays in 64 bits
static_assert(int64_t{HAZARD_STORM_MAX_DRIFT_MM_S} * HAZARD_STORM_MAX_SOLS *
                  MARS_SOL_S <
              INT64_MAX / Q16_ONE);
// The reach of a storm over its whole life stays in 64 bits
static_assert(uint64_t{2} * HAZARD_STORM_MAX_DRIFT_MM_S * HAZARD_STORM_MAX_SOLS *
                      MARS_SOL_S + UINT32_MAX <
              UINT64_MAX / 2);
static_assert(sizeof(t_route_hazard) == 16);
static_assert(GROUND_SOFT_MAX_TIU < GROUND_DUSTY_MAX_TIU);
static_assert((HAZARD_DANGER_REASONS & HAZARD_CAUTION_REASONS) == 0,
              "a reason has one level");
static_assert(RADIATION_CAUTION_USV_DAY < RADIATION_CRITICAL_USV_DAY);
static_assert(HAZARD_SLOPE_DANGER_HALF_DEG <= LANDFORM_MAX_SLOPE_HALF_DEG);
// The MOLA layer must count as coarse, a 20 m CTX layer as fine
static_assert(LANDFORM_MOLA_STEP_UDEG > HAZARD_FINE_TERRAIN_MAX_STEP_UDEG);
// u64 dose accumulator: rate <= u16, seconds <= 136 years in u32 per point
static_assert(uint64_t{UINT16_MAX} * UINT32_MAX < UINT64_MAX / UINT16_MAX);
