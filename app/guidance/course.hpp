#pragma once

#include "../core/gps/Position.hpp"
#include "../data/core/coord.hpp"
#include <cstdint>

// Heading from the GPS fixes (course over ground, true north): Mars has no
// global magnetic field for a compass.

// Constants //
constexpr uint32_t COURSE_MIN_MOVE_MM = 5000;
constexpr uint32_t COURSE_STALE_S = 120;

// Structures //
struct s_course {
  t_coord anchor;          // fix the next move is measured from
  uint32_t anchor_time_s;
  uint32_t moved_time_s;   // time of the last move that set the heading
  uint16_t heading_deg;    // 0 = north, 90 = east
  uint16_t speed_mm_s;
  bool has_anchor;
  bool valid;              // heading_deg and speed_mm_s are known
} typedef t_course;

// Functions //
void course_reset(t_course *course);
void course_update(t_course *course, mars::t_position position, uint32_t time_s);
bool course_known(const t_course *course, uint32_t time_s);
