#pragma once

#include "../core/gps/Position.hpp"
#include "../data/core/coord.hpp"
#include <cstdint>

// Turn-by-turn guidance along a planned route, the way a road navigator does
// it on Earth:
//   - the route is cut into legs of steady heading; a change of heading of
//     GUIDANCE_STRAIGHT_DEG or more is a manoeuvre (turn left, right...)
//   - each GPS fix is snapped to the route: distance done, to the next
//     manoeuvre and to the destination, time left at the walking speed
//   - straying from the route for a few fixes in a row asks for a new route;
//     reaching its end is the arrival
//   - the next hazard on the way ahead is assessed at its time of passage
// Mars has no roads: a manoeuvre names the new heading ("then head NE").

// Constants //
constexpr uint32_t GUIDANCE_MAX_POINTS = 4096;
constexpr uint32_t GUIDANCE_MAX_STEPS = 128;
constexpr uint32_t DEG_PER_TURN = 360;
constexpr int32_t HALF_TURN_DEG = 180;
// Headings are measured over this length: the planner's path has small
// zigzags that a walker does not follow one by one
constexpr uint32_t GUIDANCE_HEADING_BASE_MM = 60000;
// Two changes of heading closer than this are one manoeuvre (a corner)
constexpr uint32_t GUIDANCE_MERGE_MM = 2 * GUIDANCE_HEADING_BASE_MM;
// Heading changes, in degrees: under STRAIGHT the walker keeps going
constexpr int32_t GUIDANCE_STRAIGHT_DEG = 25;
constexpr int32_t GUIDANCE_SLIGHT_DEG = 50;
constexpr int32_t GUIDANCE_SHARP_DEG = 130;
constexpr int32_t GUIDANCE_U_TURN_DEG = 165;
// A fix farther than this from the route is off it: GPS error plus the
// 50 m safety margin the planner keeps from hazards
constexpr uint32_t GUIDANCE_OFF_ROUTE_MM = 50000;
constexpr uint8_t GUIDANCE_OFF_ROUTE_FIXES = 3;
// Closer than this to the end of the route: arrived
constexpr uint32_t GUIDANCE_ARRIVAL_MM = 15000;
// Hazards are looked for this far ahead, every LOOKAHEAD_STEP
constexpr uint32_t GUIDANCE_LOOKAHEAD_MM = 500000;
constexpr uint32_t GUIDANCE_LOOKAHEAD_STEP_MM = 20000;
// Snapping first searches this many points around the last match
constexpr uint32_t GUIDANCE_SNAP_WINDOW = 64;

// Enumerations //
enum e_maneuver {
  MANEUVER_DEPART,
  MANEUVER_SLIGHT_LEFT,
  MANEUVER_SLIGHT_RIGHT,
  MANEUVER_LEFT,
  MANEUVER_RIGHT,
  MANEUVER_SHARP_LEFT,
  MANEUVER_SHARP_RIGHT,
  MANEUVER_U_TURN,
  MANEUVER_ARRIVE
};

enum e_guidance_state {
  GUIDANCE_IDLE,
  GUIDANCE_ON_ROUTE,
  GUIDANCE_OFF_ROUTE, // ask the planner for a new route from here
  GUIDANCE_ARRIVED
};

// Structures //
struct s_guidance_step {
  uint32_t at_mm;       // distance along the route of the manoeuvre
  uint16_t heading_deg; // heading of the leg it starts, 0 = north, clockwise
  uint8_t maneuver;     // e_maneuver
} typedef t_guidance_step;

// What to tell the walker after a fix
struct s_guidance_fix {
  mars::t_position snapped; // the fix moved onto the route
  uint32_t along_mm;        // distance walked along the route
  uint32_t to_maneuver_mm;  // distance to the next manoeuvre
  uint32_t remaining_mm;    // distance to the destination
  uint32_t remaining_s;     // walking time to the destination
  uint32_t off_route_mm;    // distance from the fix to the route
  uint32_t to_hazard_mm;    // distance to the next hazard, if hazard_level
  uint16_t heading_deg;     // heading to follow now
  uint16_t next_heading_deg; // heading after the next manoeuvre
  uint16_t hazard_reasons;  // HAZARD_* bits of the next hazard
  uint8_t state;            // e_guidance_state
  uint8_t maneuver;         // e_maneuver, the next one
  uint8_t hazard_level;     // e_hazard_level of the next hazard, SAFE if none
} typedef t_guidance_fix;

struct s_guidance {
  t_coord points[GUIDANCE_MAX_POINTS];
  uint32_t along_mm[GUIDANCE_MAX_POINTS]; // distance from the start
  t_guidance_step steps[GUIDANCE_MAX_STEPS];
  uint32_t count;
  uint32_t step_count;
  uint32_t last_index; // segment of the last snap
  uint32_t speed_mm_s;
  uint8_t off_route_fixes;
  uint8_t state;       // e_guidance_state
} typedef t_guidance;

// Functions //
// Copies the route (start first, at least 2 points). Longer routes are
// thinned to GUIDANCE_MAX_POINTS. speed_mm_s: walking speed for the times.
bool guidance_start(t_guidance *guidance, const mars::t_position *route,
                    uint32_t count, uint32_t speed_mm_s);
void guidance_stop(t_guidance *guidance);
uint32_t guidance_length_mm(const t_guidance *guidance);
// Snaps a GPS fix and tells what comes next. Hazards ahead are assessed by
// the data module at their time of passage, from `time_s` on.
t_guidance_fix guidance_update(t_guidance *guidance, mars::t_position position,
                               uint32_t time_s);
// Point of the route at a distance from its start (clamped to its ends)
t_coord guidance_point_at(const t_guidance *guidance, uint32_t along_mm);
// Heading from a to b, degrees clockwise from north, in [0, 360)
uint16_t guidance_heading_deg(t_coord a, t_coord b);
// Signed change from one heading to another, in (-180, 180]: > 0 = right
int32_t guidance_turn_deg(uint16_t from_deg, uint16_t to_deg);
// "N", "NE", "E"... the nearest of the 8 compass points
const char *guidance_compass(uint16_t heading_deg);
mars::t_position guidance_position_of(t_coord coord);
