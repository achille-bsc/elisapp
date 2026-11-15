#include "core/gps/packet/MarsPositionPacket.hpp"
#include "data/data.hpp"
#include "core/route/TripPlanner.hpp"
#include "guidance/course.hpp"
#include "guidance/guidance.hpp"
#include "integration/DataStormForecast.hpp"
#include "integration/DataEnvironment.hpp"
#include "integration/MarspathRoutePlanner.hpp"
#include "platform/gps_sim.hpp"
#include "test_zone.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

// Tests of the guidance, the simulated GPS and the whole chain without a
// screen: the team's planner on the data module, walked by the simulated
// receiver and followed by the guidance. Run: make test-app

static int g_failed = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_checks++;                                                                \
    if (!(cond)) {                                                             \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
      g_failed++;                                                              \
    }                                                                          \
  } while (0)

static bool near(double a, double b, double tolerance) {
  return (std::fabs(a - b) <= tolerance);
}

// Every test position is metres east / north of this base
static const mars::t_position BASE = {180000000, 770000000}; // 18 N, 77 E
constexpr double MM_PER_M = 1000.0;
constexpr uint32_t SPEED_MM_S = WALK_SPEED_MM_S;
// The planner samples its paths about every 50 m
constexpr double SAMPLE_M = 50.0;
// Headings and turn positions are measured over 60 m stretches
constexpr double TURN_TOLERANCE_M = 70.0;
constexpr int32_t HEADING_TOLERANCE_DEG = 3;

static mars::t_position at(double east_m, double north_m) {
  return (test_offset(BASE, east_m, north_m));
}

// Straight legs through the corners, one point every SAMPLE_M
static std::vector<mars::t_position> polyline(std::vector<std::pair<double, double>> corners) {
  std::vector<mars::t_position> route;
  for (size_t i = 0; i + 1 < corners.size(); i++) {
    const double dx = corners[i + 1].first - corners[i].first;
    const double dy = corners[i + 1].second - corners[i].second;
    const int32_t steps = static_cast<int32_t>(std::ceil(std::hypot(dx, dy) / SAMPLE_M));
    for (int32_t k = 0; k < steps; k++)
      route.push_back(at(corners[i].first + dx * k / steps, corners[i].second + dy * k / steps));
  }
  route.push_back(at(corners.back().first, corners.back().second));
  return (route);
}

static bool heading_near(uint16_t heading, uint16_t expected) {
  const int32_t turn = guidance_turn_deg(heading, expected);
  return (turn <= HEADING_TOLERANCE_DEG && turn >= -HEADING_TOLERANCE_DEG);
}

static t_guidance g_guidance; // big: static storage

// Helpers //
static void test_helpers() {
  CHECK(guidance_turn_deg(350, 10) == 20 && guidance_turn_deg(10, 350) == -20);
  CHECK(guidance_turn_deg(0, 180) == 180 && guidance_turn_deg(90, 270) == 180);
  CHECK(guidance_turn_deg(45, 45) == 0);
  CHECK(std::string_view(guidance_compass(0)) == "N");
  CHECK(std::string_view(guidance_compass(44)) == "NE");
  CHECK(std::string_view(guidance_compass(90)) == "E");
  CHECK(std::string_view(guidance_compass(200)) == "S");
  CHECK(std::string_view(guidance_compass(359)) == "N");
  const t_coord base = coord_from_e7_east(BASE.latitudeE7, BASE.longitudeEastE7);
  const mars::t_position n = at(0, 500);
  const mars::t_position e = at(500, 0);
  CHECK(heading_near(guidance_heading_deg(base, coord_from_e7_east(n.latitudeE7, n.longitudeEastE7)), 0));
  CHECK(heading_near(guidance_heading_deg(base, coord_from_e7_east(e.latitudeE7, e.longitudeEastE7)), 90));
  CHECK(guidance_heading_deg(base, base) == 0);
  CHECK(near(test_distance_m(BASE, at(300, 400)), 500, 1));
}

// Route geometry: legs and manoeuvres //
static void check_turn(std::vector<std::pair<double, double>> corners, uint8_t maneuver,
                       uint16_t new_heading) {
  const std::vector<mars::t_position> route = polyline(corners);
  CHECK(guidance_start(&g_guidance, route.data(), static_cast<uint32_t>(route.size()), SPEED_MM_S));
  CHECK(g_guidance.step_count == 3); // depart, turn, arrive
  CHECK(g_guidance.steps[1].maneuver == maneuver);
  CHECK(heading_near(g_guidance.steps[1].heading_deg, new_heading));
}

static void test_steps() {
  landform_clear();
  CHECK(!guidance_start(&g_guidance, nullptr, 2, SPEED_MM_S));
  const mars::t_position one[] = {BASE};
  CHECK(!guidance_start(&g_guidance, one, 1, SPEED_MM_S));
  const mars::t_position two[] = {BASE, at(0, 100)};
  CHECK(!guidance_start(&g_guidance, two, 2, 0));
  CHECK(guidance_start(&g_guidance, two, 2, SPEED_MM_S));
  CHECK(g_guidance.step_count == 2 && g_guidance.steps[1].maneuver == MANEUVER_ARRIVE);

  // L: 1 km north then 1 km east
  const std::vector<mars::t_position> l = polyline({{0, 0}, {0, 1000}, {1000, 1000}});
  CHECK(guidance_start(&g_guidance, l.data(), static_cast<uint32_t>(l.size()), SPEED_MM_S));
  CHECK(near(guidance_length_mm(&g_guidance) / MM_PER_M, 2000, 2));
  CHECK(g_guidance.step_count == 3);
  CHECK(g_guidance.steps[0].maneuver == MANEUVER_DEPART && heading_near(g_guidance.steps[0].heading_deg, 0));
  CHECK(g_guidance.steps[1].maneuver == MANEUVER_RIGHT && heading_near(g_guidance.steps[1].heading_deg, 90));
  CHECK(near(g_guidance.steps[1].at_mm / MM_PER_M, 1000, TURN_TOLERANCE_M));
  CHECK(g_guidance.steps[2].maneuver == MANEUVER_ARRIVE &&
        g_guidance.steps[2].at_mm == guidance_length_mm(&g_guidance));

  check_turn({{0, 0}, {0, 1000}, {-1000, 1000}}, MANEUVER_LEFT, 270);
  // 35 deg to the right: bear right
  check_turn({{0, 0}, {0, 1000}, {574, 1819}}, MANEUVER_SLIGHT_RIGHT, 35);
  // 145 deg to the left: sharp left
  check_turn({{0, 0}, {0, 1000}, {-574, 181}}, MANEUVER_SHARP_LEFT, 215);
  check_turn({{0, 0}, {0, 1000}, {30, 0}}, MANEUVER_U_TURN, 178);

  // +-5 m zigzags along a straight line are not manoeuvres
  std::vector<mars::t_position> zigzag;
  constexpr double ZIG_STEP_M = 25;
  constexpr double ZIG_M = 5;
  for (int32_t k = 0; k <= 80; k++)
    zigzag.push_back(at(k % 2 ? ZIG_M : -ZIG_M, k * ZIG_STEP_M));
  CHECK(guidance_start(&g_guidance, zigzag.data(), static_cast<uint32_t>(zigzag.size()), SPEED_MM_S));
  CHECK(g_guidance.step_count == 2);

  // A route longer than the copy is thinned, both ends kept
  std::vector<mars::t_position> dense;
  constexpr uint32_t DENSE_POINTS = 10000;
  for (uint32_t k = 0; k < DENSE_POINTS; k++)
    dense.push_back(at(0, k * 0.5));
  CHECK(guidance_start(&g_guidance, dense.data(), DENSE_POINTS, SPEED_MM_S));
  CHECK(g_guidance.count == GUIDANCE_MAX_POINTS);
  const mars::t_position end = guidance_position_of(g_guidance.points[GUIDANCE_MAX_POINTS - 1]);
  CHECK(near(test_distance_m(end, dense.back()), 0, 0.01));
  CHECK(near(guidance_length_mm(&g_guidance) / MM_PER_M, (DENSE_POINTS - 1) * 0.5, 1));

  // Points along the route
  CHECK(guidance_start(&g_guidance, l.data(), static_cast<uint32_t>(l.size()), SPEED_MM_S));
  const t_coord start = guidance_point_at(&g_guidance, 0);
  CHECK(near(test_distance_m(guidance_position_of(start), BASE), 0, 0.01));
  CHECK(near(test_distance_m(guidance_position_of(guidance_point_at(&g_guidance, 500000)), at(0, 500)), 0, 1));
  CHECK(near(test_distance_m(guidance_position_of(guidance_point_at(&g_guidance, UINT32_MAX)), at(1000, 1000)), 0, 0.01));
}

// Following the route //
static void test_fixes() {
  landform_clear();
  const std::vector<mars::t_position> l = polyline({{0, 0}, {0, 1000}, {1000, 1000}});
  CHECK(guidance_start(&g_guidance, l.data(), static_cast<uint32_t>(l.size()), SPEED_MM_S));

  t_guidance_fix fix = guidance_update(&g_guidance, at(0, 200), 0);
  CHECK(fix.state == GUIDANCE_ON_ROUTE);
  CHECK(near(fix.along_mm / MM_PER_M, 200, 1));
  CHECK(near(fix.to_maneuver_mm / MM_PER_M, 800, TURN_TOLERANCE_M));
  CHECK(fix.maneuver == MANEUVER_RIGHT && heading_near(fix.next_heading_deg, 90));
  CHECK(heading_near(fix.heading_deg, 0));
  CHECK(near(fix.remaining_mm / MM_PER_M, 1800, 1));
  CHECK(fix.remaining_s == fix.remaining_mm / SPEED_MM_S);
  CHECK(near(test_distance_m(fix.snapped, at(0, 200)), 0, 1));

  // 30 m beside the route: still on it, snapped onto it
  fix = guidance_update(&g_guidance, at(30, 500), 0);
  CHECK(fix.state == GUIDANCE_ON_ROUTE && near(fix.off_route_mm / MM_PER_M, 30, 1));
  CHECK(near(test_distance_m(fix.snapped, at(0, 500)), 0, 1));

  // 200 m away: off route on the third fix in a row only
  CHECK(guidance_update(&g_guidance, at(200, 500), 0).state == GUIDANCE_ON_ROUTE);
  CHECK(guidance_update(&g_guidance, at(200, 500), 0).state == GUIDANCE_ON_ROUTE);
  CHECK(guidance_update(&g_guidance, at(200, 500), 0).state == GUIDANCE_OFF_ROUTE);
  CHECK(guidance_update(&g_guidance, at(0, 600), 0).state == GUIDANCE_ON_ROUTE);

  // On the east leg: the next manoeuvre is the arrival
  fix = guidance_update(&g_guidance, at(400, 1000), 0);
  CHECK(fix.maneuver == MANEUVER_ARRIVE && heading_near(fix.heading_deg, 90));
  CHECK(near(fix.to_maneuver_mm / MM_PER_M, 600, 1));

  // Arrival, which stays
  fix = guidance_update(&g_guidance, at(995, 1000), 0);
  CHECK(fix.state == GUIDANCE_ARRIVED);
  CHECK(guidance_update(&g_guidance, at(900, 1000), 0).state == GUIDANCE_ARRIVED);

  guidance_stop(&g_guidance);
  CHECK(guidance_update(&g_guidance, BASE, 0).state == GUIDANCE_IDLE);
}

// Test relief: a fine layer (~20 m cells) around the base, flat, with a wall
// of 50 deg slopes from 586 m to 646 m north, from the west edge to 600 m
// east of the base (open beyond)
constexpr uint32_t ZONE_STEP_UDEG = 337;
constexpr uint16_t ZONE_WIDTH = 280;
constexpr uint16_t ZONE_HEIGHT = 260;
constexpr double ZONE_WEST_M = -2650;
constexpr double ZONE_NORTH_M = 2600;
constexpr uint32_t WALL_FIRST_ROW = 98;
constexpr uint32_t WALL_END_ROW = 101;
constexpr uint32_t WALL_END_COL = 171;
constexpr uint8_t WALL_SLOPE_HALF_DEG = 100;
constexpr int16_t ZONE_ELEVATION_M = -2500;

static uint8_t wall_slope(uint32_t row, uint32_t col) {
  return (row >= WALL_FIRST_ROW && row < WALL_END_ROW && col < WALL_END_COL
              ? WALL_SLOPE_HALF_DEG
              : 0);
}

static std::vector<uint32_t> g_zone;

static void load_zone() {
  const mars::t_position north_west = at(ZONE_WEST_M, ZONE_NORTH_M);
  const t_coord nw = coord_from_e7_east(north_west.latitudeE7, north_west.longitudeEastE7);
  const t_landform_header header = {LANDFORM_MAGIC, nw.lon_udeg, nw.lat_udeg,   ZONE_STEP_UDEG,
                                    ZONE_WIDTH,     ZONE_HEIGHT, LANDFORM_VERSION, 0, 0};
  uint32_t size = 0;
  g_zone = test_zone_make(header, ZONE_ELEVATION_M, wall_slope, &size);
  CHECK(landform_init(g_zone.data(), size));
}

static void test_hazard_ahead() {
  load_zone();
  weather_set_link(nullptr); // no weather: information only, ignored ahead
  hazard_set_spe_alert(false);
  CHECK(data_is_hazard(coord_from_e7_east(at(0, 610).latitudeE7, at(0, 610).longitudeEastE7), 0));
  const std::vector<mars::t_position> l = polyline({{0, 0}, {0, 1000}, {1000, 1000}});
  CHECK(guidance_start(&g_guidance, l.data(), static_cast<uint32_t>(l.size()), SPEED_MM_S));
  // From 200 m north, the wall (586 m) is 386 to 406 m ahead
  t_guidance_fix fix = guidance_update(&g_guidance, at(0, 200), 0);
  CHECK(fix.hazard_level == HAZARD_DANGER && (fix.hazard_reasons & HAZARD_TOO_STEEP));
  CHECK(near(fix.to_hazard_mm / MM_PER_M, 400, GUIDANCE_LOOKAHEAD_STEP_MM / MM_PER_M));
  CHECK(!(fix.hazard_reasons & HAZARD_NO_WEATHER));
  // From the base it is beyond the 500 m look-ahead
  CHECK(guidance_update(&g_guidance, BASE, 0).hazard_level == HAZARD_SAFE);
  // Solar particle event: danger right here
  hazard_set_spe_alert(true);
  fix = guidance_update(&g_guidance, BASE, 0);
  CHECK(fix.hazard_level == HAZARD_DANGER && (fix.hazard_reasons & HAZARD_SPE_ALERT) &&
        fix.to_hazard_mm == 0);
  hazard_set_spe_alert(false);

  // The environment seen by the front
  const mars::DataEnvironment environment;
  const mars::EnvironmentSample flat = environment.sample(BASE, 0);
  CHECK(flat.hasTerrain && !flat.hasWeather && flat.hasRadiation);
  CHECK(flat.level == mars::HazardLevel::Safe && flat.elevationM == ZONE_ELEVATION_M);
  const mars::EnvironmentSample wall = environment.sample(at(0, 610), 0);
  CHECK(wall.level == mars::HazardLevel::Danger &&
        mars::hasReason(wall.reasons, mars::HazardReason::TooSteep));
}

// Simulated GPS receiver //
static void test_gps_sim() {
  gps_sim_init(BASE);
  const mars::PositionPacket::Buffer first = gps_sim_packet(0);
  const std::optional<mars::t_position> still = mars::PositionPacket::decode(first);
  CHECK(still && still->latitudeE7 == BASE.latitudeE7 && still->longitudeEastE7 == BASE.longitudeEastE7);
  const mars::PositionPacket::Buffer second = gps_sim_packet(0);
  CHECK(first != second); // the sequence count moved on

  const std::vector<mars::t_position> l = polyline({{0, 0}, {0, 1000}, {1000, 1000}});
  constexpr uint32_t START_S = 100;
  gps_sim_follow(l.data(), static_cast<uint32_t>(l.size()), SPEED_MM_S, START_S);
  CHECK(near(test_distance_m(gps_sim_position(0), BASE), 0, 0.01)); // before the start
  CHECK(near(test_distance_m(gps_sim_position(START_S + 500), at(0, 500)), 0, 1));
  CHECK(near(test_distance_m(gps_sim_position(START_S + 1500), at(500, 1000)), 0, 1));
  CHECK(near(test_distance_m(gps_sim_position(START_S + 9000), at(1000, 1000)), 0, 0.01));
  gps_sim_stop(START_S + 500);
  CHECK(near(test_distance_m(gps_sim_position(START_S + 9000), at(0, 500)), 0, 1));
}

// The whole chain: plan around the wall, walk it, follow it //
static void test_end_to_end() {
  load_zone();
  weather_set_link(nullptr);
  constexpr uint32_t DEPARTURE_S = 1000;
  constexpr double GOAL_NORTH_M = 1500;
  const mars::t_position goal = at(0, GOAL_NORTH_M);
  mars::MarspathRoutePlanner planner;
  const mars::Route planned = planner.plan(BASE, goal, DEPARTURE_S);
  CHECK(planned.summary.status == mars::RouteStatus::Ok);
  CHECK(planned.waypoints.size() >= 2);
  if (planned.waypoints.size() < 2)
    return;
  const std::vector<mars::t_position> route(planned.waypoints.begin(), planned.waypoints.end());
  CHECK(near(test_distance_m(route.front(), BASE), 0, 1));
  CHECK(near(test_distance_m(route.back(), goal), 0, 1));
  // Round the east end of the wall (592 m), with the planner's 50 m margin
  constexpr double WALL_EAST_END_M = 592;
  constexpr double PLANNER_MARGIN_M = 50;
  const t_coord base = coord_from_e7_east(BASE.latitudeE7, BASE.longitudeEastE7);
  double east_most_m = 0;
  for (const mars::t_position &p : route) {
    const t_coord c = coord_from_e7_east(p.latitudeE7, p.longitudeEastE7);
    const double east_m = static_cast<double>(lon_delta_udeg(base.lon_udeg, c.lon_udeg)) *
                          mm_per_udeg_lon_q16(base.lat_udeg) / Q16_ONE / MM_PER_M;
    east_most_m = east_m > east_most_m ? east_m : east_most_m;
  }
  CHECK(east_most_m > WALL_EAST_END_M + PLANNER_MARGIN_M / 2);
  // Any way round passes the wall end: at least base -> end -> goal
  constexpr double WALL_MIDDLE_NORTH_M = 616;
  const double via_end_m =
      std::hypot(WALL_EAST_END_M, WALL_MIDDLE_NORTH_M) +
      std::hypot(WALL_EAST_END_M, GOAL_NORTH_M - WALL_MIDDLE_NORTH_M);
  CHECK(planned.summary.lengthM >= via_end_m);
  CHECK(planned.summary.level == mars::HazardLevel::Safe);
  CHECK(near(planned.summary.walkSeconds, planned.summary.lengthM * MM_PER_M / SPEED_MM_S,
             planned.summary.walkSeconds / 50.0));
  CHECK(planned.summary.roverSeconds < planned.summary.walkSeconds);
  uint32_t on_wall = 0;
  for (const mars::t_position &p : route)
    on_wall += data_is_hazard(coord_from_e7_east(p.latitudeE7, p.longitudeEastE7), DEPARTURE_S);
  CHECK(on_wall == 0);

  CHECK(guidance_start(&g_guidance, route.data(), static_cast<uint32_t>(route.size()), SPEED_MM_S));
  CHECK(g_guidance.step_count > 2); // turns to go round the wall
  gps_sim_follow(route.data(), static_cast<uint32_t>(route.size()), SPEED_MM_S, DEPARTURE_S);
  constexpr uint32_t FIX_PERIOD_S = 5;
  uint32_t off_route = 0;
  uint32_t dangers = 0;
  uint32_t arrived_at = 0;
  for (uint32_t t = DEPARTURE_S; t < DEPARTURE_S + 2 * planned.summary.walkSeconds; t += FIX_PERIOD_S) {
    const std::optional<mars::t_position> p = mars::PositionPacket::decode(gps_sim_packet(t));
    CHECK(p.has_value());
    const t_guidance_fix fix = guidance_update(&g_guidance, *p, t);
    off_route += fix.state == GUIDANCE_OFF_ROUTE;
    dangers += fix.hazard_level == HAZARD_DANGER;
    if (fix.state == GUIDANCE_ARRIVED) {
      arrived_at = t;
      break;
    }
  }
  CHECK(off_route == 0 && dangers == 0);
  CHECK(arrived_at != 0);
  // Arrival when the walk time says so (minus the 15 m arrival radius)
  CHECK(near(arrived_at - DEPARTURE_S, planned.summary.walkSeconds, planned.summary.walkSeconds / 20.0));
}

// Course over ground: noise below 5 m is ignored, a walk gives its heading
static void test_course() {
  const mars::t_position origin = {184447000, 774508000};
  t_course course = {};
  course_reset(&course);
  course_update(&course, origin, 100);
  CHECK(!course_known(&course, 100));
  course_update(&course, test_offset(origin, 2, 2), 101); // GPS noise
  CHECK(!course_known(&course, 101));
  course_update(&course, test_offset(origin, 0, 10), 110); // 10 m north
  CHECK(course_known(&course, 110) && (course.heading_deg <= 1 || course.heading_deg >= 359));
  course_update(&course, test_offset(test_offset(origin, 0, 10), 10, 0), 120); // then east
  CHECK(course_known(&course, 120) && course.heading_deg >= 89 && course.heading_deg <= 91);
  CHECK(course.speed_mm_s >= 900 && course.speed_mm_s <= 1100);
  CHECK(!course_known(&course, 120 + COURSE_STALE_S + 1)); // standing still
  // The clock set back (new mission time): unknown until the next move
  course_update(&course, origin, 50);
  CHECK(!course_known(&course, 50));
  course_update(&course, test_offset(origin, -10, 0), 60); // 10 m west
  CHECK(course_known(&course, 60) && course.heading_deg >= 269 && course.heading_deg <= 271);
}

// A planner that walks straight lines, for the round trip
class line_planner final : public mars::IRoutePlanner {
public:
  mars::Route plan(mars::t_position from, mars::t_position to, uint32_t departure) override {
    points = {from, to};
    departures.push_back(departure);
    mars::RouteSummary summary = {};
    summary.status = to.latitudeE7 == blocked ? mars::RouteStatus::GoalInHazard : mars::RouteStatus::Ok;
    summary.walkSeconds = static_cast<uint32_t>(test_distance_m(from, to));
    summary.doseMicroSievert = 10;
    return {points, summary};
  }
  std::vector<mars::t_position> points;
  std::vector<uint32_t> departures;
  int32_t blocked = 0;
};

static void test_trip() {
  const mars::t_position base = {184447000, 774508000};
  const mars::t_position site = test_offset(base, 3000, 1000);
  line_planner lines;
  mars::TripPlanner trip{lines, EVA_MAX_DURATION_S};
  const mars::TripSummary &t = trip.plan(base, site, base, 1000, 3600);
  CHECK(t.outbound.status == mars::RouteStatus::Ok && t.back.status == mars::RouteStatus::Ok);
  // The way back leaves after the walk out and the work
  CHECK(lines.departures.size() == 2 && lines.departures[1] == 1000 + t.outbound.walkSeconds + 3600);
  CHECK(t.backDepartureSeconds == lines.departures[1]);
  CHECK(t.totalSeconds == t.outbound.walkSeconds + 3600 + t.back.walkSeconds && t.doseMicroSievert == 20);
  CHECK(t.fitsOneEva && trip.outbound().size() == 2 && trip.back().size() == 2);
  // Too long for one EVA
  const mars::TripSummary &far = trip.plan(base, site, base, 0, EVA_MAX_DURATION_S);
  CHECK(!far.fitsOneEva);
  // No way back: the trip does not fit, the way out still shows
  lines.blocked = base.latitudeE7;
  const mars::TripSummary &stuck = trip.plan(base, site, base, 0, 0);
  CHECK(stuck.outbound.status == mars::RouteStatus::Ok && stuck.back.status != mars::RouteStatus::Ok);
  CHECK(!stuck.fitsOneEva && trip.back().empty());
  // From the site, only the way back: planned from there, leaving now
  lines.blocked = 0;
  const mars::TripSummary &home = trip.planBack(site, base, 5000);
  CHECK(home.back.status == mars::RouteStatus::Ok && home.backDepartureSeconds == 5000);
  CHECK(lines.departures.back() == 5000 && trip.back().size() == 2 && trip.back()[0].latitudeE7 == site.latitudeE7);
}

// Metres east and north of BASE (local flat projection)
static std::pair<double, double> local_m(mars::t_position p) {
  const t_coord base = coord_from_e7_east(BASE.latitudeE7, BASE.longitudeEastE7);
  const t_coord c = coord_from_e7_east(p.latitudeE7, p.longitudeEastE7);
  const double east = static_cast<double>(lon_delta_udeg(base.lon_udeg, c.lon_udeg)) *
                      mm_per_udeg_lon_q16(base.lat_udeg) / Q16_ONE / MM_PER_M;
  const double north = static_cast<double>(c.lat_udeg - base.lat_udeg) * MM_PER_UDEG_LAT_Q16 / Q16_ONE / MM_PER_M;
  return {east, north};
}

// Waypoints about every 60 m, every chord 50 m clear of the wall, checked
// independently of the planner: a walk every 2 m measuring the distance to
// the wall's rectangle. Then the planner's own check of a given way.
static void test_waypoints_and_assess() {
  load_zone();
  weather_set_link(nullptr);
  hazard_set_spe_alert(false);
  CHECK(hazard_set_storms(nullptr, 0));
  constexpr uint32_t DEPARTURE_S = 1000;
  const mars::t_position goal = at(0, 1500);
  mars::MarspathRoutePlanner planner;
  const mars::Route planned = planner.plan(BASE, goal, DEPARTURE_S);
  CHECK(planned.summary.status == mars::RouteStatus::Ok && planned.waypoints.size() >= 3);
  if (planned.waypoints.size() < 3)
    return;
  const std::vector<mars::t_position> out(planned.waypoints.begin(), planned.waypoints.end());
  // The goal exactly, no point twice in a row, legs up to the spacing
  CHECK(out.back().latitudeE7 == goal.latitudeE7 && out.back().longitudeEastE7 == goal.longitudeEastE7);
  CHECK(out.front().latitudeE7 == BASE.latitudeE7 && out.front().longitudeEastE7 == BASE.longitudeEastE7);
  double longest = 0;
  double shortest = 1e9;
  double length = 0;
  double closest = 1e9;
  constexpr double WALL_EAST_M = 592; // col 171
  constexpr double WALL_SOUTH_M = 586;
  constexpr double WALL_NORTH_M = 646;
  constexpr double CELL_M = 20; // the wall's edges are known to a cell
  for (size_t i = 0; i + 1 < out.size(); i++) {
    const double leg = test_distance_m(out[i], out[i + 1]);
    longest = leg > longest ? leg : longest;
    shortest = leg < shortest ? leg : shortest;
    length += leg;
    const auto [x0, y0] = local_m(out[i]);
    const auto [x1, y1] = local_m(out[i + 1]);
    const int steps = static_cast<int>(std::ceil(leg / 2.0));
    for (int k = 0; k <= steps; k++) {
      const double x = x0 + (x1 - x0) * k / steps;
      const double y = y0 + (y1 - y0) * k / steps;
      const double dx = x > WALL_EAST_M ? x - WALL_EAST_M : 0;
      const double dy = y < WALL_SOUTH_M ? WALL_SOUTH_M - y : (y > WALL_NORTH_M ? y - WALL_NORTH_M : 0);
      const double d = std::hypot(dx, dy);
      closest = d < closest ? d : closest;
    }
  }
  CHECK(longest <= 60.5 && shortest > 0.01);
  CHECK(closest >= LANDFORM_CLEARANCE_M - CELL_M / 2);
  // The summary is the waypoints' one: what the guidance will walk
  CHECK(near(planned.summary.lengthM, length, 2));

  // A short way: start and goal, nothing in between twice
  const mars::Route shortWay = planner.plan(BASE, at(25, 0), DEPARTURE_S);
  CHECK(shortWay.summary.status == mars::RouteStatus::Ok && shortWay.waypoints.size() == 2);
  if (shortWay.waypoints.size() == 2)
    CHECK(shortWay.waypoints[1].latitudeE7 == at(25, 0).latitudeE7 &&
          near(shortWay.summary.lengthM, 25, 1));

  // The planner's check of a given way: the planned one passes, both ways
  CHECK(planner.assess(out, DEPARTURE_S).status == mars::RouteStatus::Ok);
  const std::vector<mars::t_position> reversed(out.rbegin(), out.rend());
  const mars::RouteSummary back = planner.assess(reversed, DEPARTURE_S);
  CHECK(back.status == mars::RouteStatus::Ok && back.lengthM == planned.summary.lengthM);
  CHECK(near(back.ascentM, planned.summary.descentM, 1) && near(back.descentM, planned.summary.ascentM, 1));
  // Straight through the wall, or along it 30 m off: refused
  const std::vector<mars::t_position> through = {BASE, goal};
  CHECK(planner.assess(through, DEPARTURE_S).status != mars::RouteStatus::Ok);
  const std::vector<mars::t_position> along = {at(-300, 300), at(-300, 556), at(400, 556)};
  CHECK(planner.assess(along, DEPARTURE_S).status != mars::RouteStatus::Ok);
  const std::vector<mars::t_position> clear = {at(-300, 300), at(-300, 500), at(400, 500)};
  CHECK(planner.assess(clear, DEPARTURE_S).status == mars::RouteStatus::Ok);
  const std::vector<mars::t_position> single = {BASE};
  CHECK(planner.assess(single, DEPARTURE_S).status != mars::RouteStatus::Ok);
  // A storm over the way at the time of passage only
  const t_coord over = coord_from_e7_east(at(-200, 500).latitudeE7, at(-200, 500).longitudeEastE7);
  const t_storm storm = {over, 0, 0, 100000, DEPARTURE_S + 5000, DEPARTURE_S + 9000, 1000, 120, 0};
  CHECK(hazard_set_storms(&storm, 1));
  CHECK(planner.assess(clear, DEPARTURE_S).status == mars::RouteStatus::Ok);
  CHECK(planner.assess(clear, DEPARTURE_S + 5000).status != mars::RouteStatus::Ok);
  CHECK(hazard_set_storms(nullptr, 0));

  // The round trip from the base: the same way back, ending at the base
  mars::TripPlanner trip{planner, EVA_MAX_DURATION_S, 100000};
  const mars::TripSummary &t = trip.plan(BASE, goal, BASE, DEPARTURE_S, 600);
  CHECK(t.outbound.status == mars::RouteStatus::Ok && t.back.status == mars::RouteStatus::Ok && t.sameWayBack);
  CHECK(trip.back().size() == trip.outbound().size());
  CHECK(t.back.lengthM == t.outbound.lengthM && near(t.back.ascentM, t.outbound.descentM, 1));
  // From 40 m off the base: back the same way, then on to the base itself
  const mars::t_position aside = at(-40, 0);
  const mars::TripSummary &a = trip.plan(aside, goal, BASE, DEPARTURE_S, 600);
  CHECK(a.sameWayBack && !trip.back().empty());
  if (!trip.back().empty())
    CHECK(trip.back().back().latitudeE7 == BASE.latitudeE7 && trip.back().back().longitudeEastE7 == BASE.longitudeEastE7);
  // Beyond 100 m of the base: a way back planned from the site
  const mars::TripSummary &f = trip.plan(at(-300, 0), goal, BASE, DEPARTURE_S, 600);
  CHECK(f.back.status == mars::RouteStatus::Ok && !f.sameWayBack);
  // A planner that cannot check a way: planned back
  line_planner lines;
  mars::TripPlanner simple{lines, EVA_MAX_DURATION_S, 100000};
  CHECK(!simple.plan(BASE, goal, BASE, DEPARTURE_S, 600).sameWayBack && lines.departures.size() == 2);
}

// Caution areas: the planner goes round one when the detour is short
struct flat_terrain final : public marspath::ITerrain {
  [[nodiscard]] float elevation(marspath::GeoPoint) const override { return (0.0F); }
};

struct caution_disc final : public marspath::IRiskMap {
  [[nodiscard]] bool isHazard(marspath::GeoPoint) const override { return (false); }
  [[nodiscard]] marspath::RiskLevel level(marspath::GeoPoint p) const override {
    return (inside(p) ? marspath::RiskLevel::Caution : marspath::RiskLevel::Safe);
  }
  // Equator, 1e-7 deg ~ 5.9 mm: plain metres around the centre
  [[nodiscard]] bool inside(marspath::GeoPoint p) const {
    constexpr double M_PER_E7 = 59157.6 / 1e7;
    const double dx = (p.lon_e7 - centre_lon_e7) * M_PER_E7;
    const double dy = p.lat_e7 * M_PER_E7;
    return (dx * dx + dy * dy < radius_m * radius_m);
  }
  int32_t centre_lon_e7 = 0;
  double radius_m = 0;
};

static size_t samples_inside(const marspath::PlanResult &r, const caution_disc &disc) {
  size_t n = 0;
  for (size_t i = 1; i < r.path.size(); i++)
    for (int k = 0; k < 10; k++) {
      const marspath::GeoPoint a = r.path[i - 1].position;
      const marspath::GeoPoint b = r.path[i].position;
      const marspath::GeoPoint p = {a.lat_e7 + (b.lat_e7 - a.lat_e7) * k / 10,
                                    a.lon_e7 + (b.lon_e7 - a.lon_e7) * k / 10};
      n += disc.inside(p);
    }
  return (n);
}

static void test_caution_detour() {
  // 5 km due east, a 400 m caution disc right on the straight line
  const marspath::GeoPoint start = {0, 100'000'000};
  const marspath::GeoPoint goal = {0, 100'845'000};
  caution_disc disc;
  disc.centre_lon_e7 = (start.lon_e7 + goal.lon_e7) / 2;
  disc.radius_m = 400;
  const flat_terrain flat;
  marspath::PlannerParams through;
  through.cost.caution_penalty = 0.0;
  const marspath::PlanResult straight = marspath::Planner{disc, flat, through}.plan(start, goal);
  CHECK(straight.ok() && samples_inside(straight, disc) > 0);
  const marspath::PlanResult round = marspath::Planner{disc, flat}.plan(start, goal);
  CHECK(round.ok() && samples_inside(round, disc) == 0);
  // Not a long way round: the disc is small
  CHECK(round.ok() && round.stats.length_m < straight.stats.length_m * 1.15);
}

static void test_storm_forecast() {
  const char *path = ".tmp/storm_test.json";
  {
    std::ofstream file(path);
    file << R"({"source": "Test bulletin", "storms": [{"name": "S1", "latitude": 18.44,
      "longitude": 77.45, "radius_km": 2, "start_in_hours": 1, "duration_hours": 2,
      "drift_east_m_s": 0, "drift_north_m_s": 0, "dust_opacity": 3, "wind_m_s": 30}]})";
  }
  mars::DataStormForecast storms;
  CHECK(storms.load(path, 1000));
  CHECK(std::string_view{storms.source()} == "Test bulletin" && hazard_storm_count() == 1);
  const t_coord centre = coord_from_e7_east(184400000, 774500000);
  CHECK(!hazard_storm_at(centre, 1000));
  CHECK(hazard_storm_at(centre, 1000 + MARS_HOUR_S) != nullptr);
  CHECK(hazard_assess(centre, 1000 + MARS_HOUR_S).reasons & HAZARD_WIND);
  CHECK(storms.at(1000).size() == 1 && !storms.at(1000)[0].active);
  CHECK(storms.at(1000 + MARS_HOUR_S)[0].active && storms.at(1000 + MARS_HOUR_S)[0].radiusM == 2000);
  CHECK(storms.at(1000 + 3 * MARS_HOUR_S).empty()); // over
  CHECK(hazard_storm_during(centre, 0, 1000 + MARS_HOUR_S) != nullptr);
  CHECK(hazard_storm_during(centre, 0, 999) == nullptr);
  {
    std::ofstream file(path);
    file << R"({"storms": [{"name": "bad", "latitude": 95, "longitude": 0, "radius_km": 1,
      "start_in_hours": 0, "duration_hours": 1}]})";
  }
  CHECK(!storms.load(path, 0) && hazard_storm_count() == 0);
  {
    // A drift far beyond any int32 of mm/s: refused, not overflowed
    std::ofstream file(path);
    file << R"({"storms": [{"name": "fast", "latitude": 18, "longitude": 77, "radius_km": 1,
      "start_in_hours": 0, "duration_hours": 1, "drift_east_m_s": -1e12}]})";
  }
  CHECK(!storms.load(path, 0) && hazard_storm_count() == 0);
  CHECK(!storms.load("missing.json", 0));
  std::remove(path);
}

int main() {
  test_caution_detour();
  test_helpers();
  test_steps();
  test_fixes();
  test_hazard_ahead();
  test_gps_sim();
  test_end_to_end();
  test_course();
  test_trip();
  test_waypoints_and_assess();
  test_storm_forecast();
  if (g_failed)
    printf("%d / %d check(s) FAILED\n", g_failed, g_checks);
  else
    printf("all %d guidance checks passed\n", g_checks);
  return (g_failed != 0);
}

static_assert(WALL_END_ROW <= ZONE_HEIGHT && WALL_END_COL <= ZONE_WIDTH);
static_assert(ZONE_STEP_UDEG <= HAZARD_FINE_TERRAIN_MAX_STEP_UDEG, "fine layer");
static_assert(WALL_SLOPE_HALF_DEG > HAZARD_SLOPE_DANGER_HALF_DEG);
