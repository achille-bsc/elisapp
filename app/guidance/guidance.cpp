#include "./guidance.hpp"
#include "../data/core/geodesy.hpp"
#include "../data/hazard/hazard.hpp"
#include <cmath>
#include <type_traits>

// Hazards a walker cannot do anything about on the way: relief known at
// 1.85 km only (the whole MOLA zone) and a missing satellite pass
constexpr uint16_t GUIDANCE_IGNORED_REASONS =
    HAZARD_COARSE_TERRAIN | HAZARD_NO_WEATHER;
constexpr uint32_t COMPASS_POINTS = 8;
constexpr double RAD_TO_DEG = DEG_PER_HALF_TURN / PI;

static const char *const COMPASS_NAMES[COMPASS_POINTS] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"};

// Position of p in millimetres east and north of origin
static void local_mm(t_coord origin, t_coord p, double *x, double *y) {
  *x = static_cast<double>(lon_delta_udeg(origin.lon_udeg, p.lon_udeg)) *
       mm_per_udeg_lon_q16(origin.lat_udeg) / Q16_ONE;
  *y = static_cast<double>(int64_t{p.lat_udeg} - origin.lat_udeg) *
       MM_PER_UDEG_LAT_Q16 / Q16_ONE;
}

mars::t_position guidance_position_of(t_coord coord) {
  return (mars::t_position{coord_lat_e7(coord),
                           coord.lon_udeg * static_cast<uint32_t>(E7_PER_UDEG)});
}

uint16_t guidance_heading_deg(t_coord a, t_coord b) {
  double x = 0;
  double y = 0;
  local_mm(a, b, &x, &y);
  if (x == 0 && y == 0)
    return (0);
  const double deg = std::atan2(x, y) * RAD_TO_DEG;
  const int32_t rounded = static_cast<int32_t>(std::lround(deg));
  const int32_t turn = static_cast<int32_t>(DEG_PER_TURN);
  return (static_cast<uint16_t>(((rounded % turn) + turn) % turn));
}

int32_t guidance_turn_deg(uint16_t from_deg, uint16_t to_deg) {
  const int32_t turn = static_cast<int32_t>(DEG_PER_TURN);
  int32_t diff = (int32_t{to_deg} - int32_t{from_deg}) % turn;
  if (diff > HALF_TURN_DEG)
    diff -= turn;
  else if (diff <= -HALF_TURN_DEG)
    diff += turn;
  return (diff);
}

const char *guidance_compass(uint16_t heading_deg) {
  // Each compass point covers 45 deg centred on it
  const uint32_t sector = DEG_PER_TURN / COMPASS_POINTS;
  const uint32_t index =
      (heading_deg % DEG_PER_TURN + sector / 2) / sector % COMPASS_POINTS;
  return (COMPASS_NAMES[index]);
}

static uint8_t classify(int32_t turn_deg) {
  const int32_t amount = turn_deg < 0 ? -turn_deg : turn_deg;
  const bool right = turn_deg > 0;
  if (amount >= GUIDANCE_U_TURN_DEG)
    return (MANEUVER_U_TURN);
  if (amount >= GUIDANCE_SHARP_DEG)
    return (right ? MANEUVER_SHARP_RIGHT : MANEUVER_SHARP_LEFT);
  if (amount >= GUIDANCE_SLIGHT_DEG)
    return (right ? MANEUVER_RIGHT : MANEUVER_LEFT);
  return (right ? MANEUVER_SLIGHT_RIGHT : MANEUVER_SLIGHT_LEFT);
}

uint32_t guidance_length_mm(const t_guidance *guidance) {
  return (guidance->count < 2 ? 0 : guidance->along_mm[guidance->count - 1]);
}

t_coord guidance_point_at(const t_guidance *guidance, uint32_t along_mm) {
  if (guidance->count == 0)
    return (t_coord{});
  if (along_mm >= guidance_length_mm(guidance))
    return (guidance->points[guidance->count - 1]);
  // Last point at or before along_mm (binary search on the distances)
  uint32_t lo = 0;
  uint32_t hi = guidance->count - 1;
  while (hi - lo > 1) {
    const uint32_t mid = lo + (hi - lo) / 2;
    if (guidance->along_mm[mid] <= along_mm)
      lo = mid;
    else
      hi = mid;
  }
  const t_coord a = guidance->points[lo];
  const t_coord b = guidance->points[hi];
  const int64_t done = along_mm - guidance->along_mm[lo];
  const int64_t span = guidance->along_mm[hi] - guidance->along_mm[lo];
  if (span == 0)
    return (a);
  const int64_t lon = int64_t{a.lon_udeg} +
                      int64_t{lon_delta_udeg(a.lon_udeg, b.lon_udeg)} * done / span;
  const int64_t lat =
      int64_t{a.lat_udeg} + (int64_t{b.lat_udeg} - a.lat_udeg) * done / span;
  return (t_coord{lon_wrap_udeg(lon), static_cast<int32_t>(lat)});
}

static void add_step(t_guidance *guidance, uint32_t at_mm, uint16_t heading,
                     uint8_t maneuver) {
  // The last slot stays free for the arrival
  if (guidance->step_count + 1 >= GUIDANCE_MAX_STEPS && maneuver != MANEUVER_ARRIVE)
    return;
  guidance->steps[guidance->step_count++] =
      t_guidance_step{at_mm, heading, maneuver};
}

// Heading of the stretch [at, at + base]
static uint16_t heading_at(const t_guidance *guidance, uint32_t at_mm) {
  return (guidance_heading_deg(guidance_point_at(guidance, at_mm),
                               guidance_point_at(guidance, at_mm +
                                                               GUIDANCE_HEADING_BASE_MM)));
}

static void build_steps(t_guidance *guidance) {
  const uint32_t total = guidance_length_mm(guidance);
  uint16_t leg = total < GUIDANCE_HEADING_BASE_MM
                     ? guidance_heading_deg(guidance->points[0],
                                            guidance->points[guidance->count - 1])
                     : heading_at(guidance, 0);
  add_step(guidance, 0, leg, MANEUVER_DEPART);
  uint16_t before_turn = leg; // heading before the last manoeuvre
  for (uint32_t at = GUIDANCE_HEADING_BASE_MM;
       at + GUIDANCE_HEADING_BASE_MM <= total; at += GUIDANCE_HEADING_BASE_MM) {
    const uint16_t heading = heading_at(guidance, at);
    const int32_t turn = guidance_turn_deg(leg, heading);
    if (turn < GUIDANCE_STRAIGHT_DEG && turn > -GUIDANCE_STRAIGHT_DEG)
      continue;
    t_guidance_step &last = guidance->steps[guidance->step_count - 1];
    if (last.maneuver != MANEUVER_DEPART && at - last.at_mm < GUIDANCE_MERGE_MM) {
      // Same corner: one manoeuvre from the heading before it
      const int32_t whole = guidance_turn_deg(before_turn, heading);
      last.heading_deg = heading;
      last.maneuver = classify(whole);
      if (whole < GUIDANCE_STRAIGHT_DEG && whole > -GUIDANCE_STRAIGHT_DEG)
        guidance->step_count--; // the two changes cancel out
    } else {
      before_turn = leg;
      add_step(guidance, at, heading, classify(turn));
    }
    leg = heading;
  }
  add_step(guidance, total, leg, MANEUVER_ARRIVE);
}

bool guidance_start(t_guidance *guidance, const mars::t_position *route,
                    uint32_t count, uint32_t speed_mm_s) {
  guidance_stop(guidance);
  if (!route || count < 2 || speed_mm_s == 0)
    return (false);
  const uint32_t kept = count > GUIDANCE_MAX_POINTS ? GUIDANCE_MAX_POINTS : count;
  // In double: rounding each segment down to the mm would add up to metres
  double along = 0;
  for (uint32_t i = 0; i < kept; i++) {
    // Thinning keeps both ends: index 0 and count - 1
    const uint32_t from = static_cast<uint32_t>(
        uint64_t{i} * (count - 1) / (kept - 1));
    const t_coord c = coord_from_e7_east(route[from].latitudeE7,
                                         route[from].longitudeEastE7);
    if (i > 0) {
      double x = 0;
      double y = 0;
      local_mm(guidance->points[i - 1], c, &x, &y);
      along += std::sqrt(x * x + y * y);
    }
    guidance->points[i] = c;
    guidance->along_mm[i] = static_cast<uint32_t>(
        along > UINT32_MAX ? UINT32_MAX : std::lround(along));
  }
  guidance->count = kept;
  guidance->speed_mm_s = speed_mm_s;
  guidance->state = GUIDANCE_ON_ROUTE;
  build_steps(guidance);
  return (true);
}

void guidance_stop(t_guidance *guidance) {
  guidance->count = 0;
  guidance->step_count = 0;
  guidance->last_index = 0;
  guidance->off_route_fixes = 0;
  guidance->state = GUIDANCE_IDLE;
}

struct s_snap {
  double distance_mm;
  uint32_t along_mm;
  uint32_t segment;
} typedef t_snap;

// Closest point of segment i to p
static t_snap snap_segment(const t_guidance *guidance, uint32_t i, t_coord p) {
  double bx = 0, by = 0, px = 0, py = 0;
  local_mm(guidance->points[i], guidance->points[i + 1], &bx, &by);
  local_mm(guidance->points[i], p, &px, &py);
  const double length2 = bx * bx + by * by;
  double t = length2 > 0 ? (px * bx + py * by) / length2 : 0;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const double dx = px - t * bx;
  const double dy = py - t * by;
  const uint32_t span = guidance->along_mm[i + 1] - guidance->along_mm[i];
  return (t_snap{std::sqrt(dx * dx + dy * dy),
                 guidance->along_mm[i] +
                     static_cast<uint32_t>(std::lround(t * span)),
                 i});
}

static t_snap snap_range(const t_guidance *guidance, uint32_t first,
                         uint32_t end, t_coord p) {
  t_snap best = snap_segment(guidance, first, p);
  for (uint32_t i = first + 1; i < end; i++) {
    const t_snap s = snap_segment(guidance, i, p);
    if (s.distance_mm < best.distance_mm)
      best = s;
  }
  return (best);
}

static t_snap snap(const t_guidance *guidance, t_coord p) {
  const uint32_t segments = guidance->count - 1;
  const uint32_t last = guidance->last_index;
  const uint32_t first = last > GUIDANCE_SNAP_WINDOW ? last - GUIDANCE_SNAP_WINDOW : 0;
  const uint32_t end = last + GUIDANCE_SNAP_WINDOW < segments
                           ? last + GUIDANCE_SNAP_WINDOW
                           : segments;
  const t_snap near = snap_range(guidance, first, end, p);
  // Lost near the last match (jump, new route...): search the whole route
  if (near.distance_mm <= GUIDANCE_OFF_ROUTE_MM)
    return (near);
  return (snap_range(guidance, 0, segments, p));
}

// First hazard ahead, at its time of passage
static void look_ahead(const t_guidance *guidance, uint32_t time_s,
                       t_guidance_fix *fix) {
  const uint32_t total = guidance_length_mm(guidance);
  const uint32_t end = total - fix->along_mm < GUIDANCE_LOOKAHEAD_MM
                           ? total
                           : fix->along_mm + GUIDANCE_LOOKAHEAD_MM;
  for (uint32_t at = fix->along_mm; at <= end; at += GUIDANCE_LOOKAHEAD_STEP_MM) {
    const uint32_t ahead_mm = at - fix->along_mm;
    const t_hazard h = hazard_assess(guidance_point_at(guidance, at),
                                     time_s + ahead_mm / guidance->speed_mm_s);
    const uint16_t reasons =
        static_cast<uint16_t>(h.reasons & ~GUIDANCE_IGNORED_REASONS);
    const uint8_t level = hazard_level_of(reasons);
    if (level != HAZARD_SAFE) {
      fix->hazard_level = level;
      fix->hazard_reasons = reasons;
      fix->to_hazard_mm = ahead_mm;
      return;
    }
  }
}

t_guidance_fix guidance_update(t_guidance *guidance, mars::t_position position,
                               uint32_t time_s) {
  t_guidance_fix fix = {};
  if (guidance->count < 2)
    return (fix);
  const t_coord p = coord_from_e7_east(position.latitudeE7, position.longitudeEastE7);
  const t_snap s = snap(guidance, p);
  guidance->last_index = s.segment;
  fix.along_mm = s.along_mm;
  fix.off_route_mm = static_cast<uint32_t>(std::lround(s.distance_mm));
  fix.snapped = guidance_position_of(guidance_point_at(guidance, s.along_mm));
  fix.remaining_mm = guidance_length_mm(guidance) - s.along_mm;
  fix.remaining_s = fix.remaining_mm / guidance->speed_mm_s;

  if (fix.off_route_mm > GUIDANCE_OFF_ROUTE_MM) {
    if (guidance->off_route_fixes < GUIDANCE_OFF_ROUTE_FIXES)
      guidance->off_route_fixes++;
  } else
    guidance->off_route_fixes = 0;
  if (guidance->state != GUIDANCE_ARRIVED) {
    if (fix.remaining_mm <= GUIDANCE_ARRIVAL_MM &&
        fix.off_route_mm <= GUIDANCE_OFF_ROUTE_MM)
      guidance->state = GUIDANCE_ARRIVED;
    else if (guidance->off_route_fixes >= GUIDANCE_OFF_ROUTE_FIXES)
      guidance->state = GUIDANCE_OFF_ROUTE;
    else
      guidance->state = GUIDANCE_ON_ROUTE;
  }
  fix.state = guidance->state;

  // Next manoeuvre: the first one strictly ahead (the arrival at worst)
  uint32_t next = 1;
  while (next + 1 < guidance->step_count &&
         guidance->steps[next].at_mm <= s.along_mm)
    next++;
  const t_guidance_step &step = guidance->steps[next];
  fix.maneuver = step.maneuver;
  fix.to_maneuver_mm = step.at_mm > s.along_mm ? step.at_mm - s.along_mm : 0;
  fix.next_heading_deg = step.heading_deg;
  fix.heading_deg = guidance->steps[next - 1].heading_deg;
  fix.hazard_level = HAZARD_SAFE;
  look_ahead(guidance, time_s, &fix);
  return (fix);
}

static_assert(sizeof(t_guidance_step) == 8);
static_assert(std::is_trivially_copyable_v<t_guidance>);
static_assert(GUIDANCE_MAX_STEPS >= 2, "departure and arrival");
static_assert(GUIDANCE_STRAIGHT_DEG < GUIDANCE_SLIGHT_DEG &&
              GUIDANCE_SLIGHT_DEG < GUIDANCE_SHARP_DEG &&
              GUIDANCE_SHARP_DEG < GUIDANCE_U_TURN_DEG &&
              GUIDANCE_U_TURN_DEG <= HALF_TURN_DEG);
static_assert(GUIDANCE_LOOKAHEAD_STEP_MM > 0 &&
              GUIDANCE_LOOKAHEAD_STEP_MM <= GUIDANCE_LOOKAHEAD_MM);
static_assert(GUIDANCE_ARRIVAL_MM < GUIDANCE_OFF_ROUTE_MM);
static_assert(DEG_PER_TURN % COMPASS_POINTS == 0);
// along_mm in 32 bits: the longest route the planner grid accepts fits
static_assert(uint64_t{GRID_MAX_LENGTH_MM} * 2 < UINT32_MAX);
