#pragma once

#include <cstdint>

// Units //
// Planetocentric coordinates in micro-degrees (1e-6 deg):
//   x = east longitude, [0, 360e6)
//   y = latitude, [-90e6, 90e6]
constexpr uint32_t UDEG_PER_DEG = 1000000;
constexpr uint32_t LON_MAX_DEG = 360;
constexpr int32_t LAT_MAX_DEG = 90;
constexpr uint32_t LON_MAX_UDEG = LON_MAX_DEG * UDEG_PER_DEG;
constexpr int32_t LAT_MAX_UDEG =
    LAT_MAX_DEG * static_cast<int32_t>(UDEG_PER_DEG);

// Structures //
struct s_coord {
  uint32_t x;
  int32_t y;
} typedef t_coord;

// Functions //
constexpr bool coord_is_valid(t_coord coord) {
  return (coord.x < LON_MAX_UDEG && coord.y >= -LAT_MAX_UDEG &&
          coord.y <= LAT_MAX_UDEG);
}

static_assert(LON_MAX_UDEG == 360000000u);
static_assert(sizeof(t_coord) == 8);
static_assert(coord_is_valid({0, 0}));
static_assert(coord_is_valid({359999999, -LAT_MAX_UDEG}));
static_assert(!coord_is_valid({LON_MAX_UDEG, 0}));
static_assert(!coord_is_valid({0, LAT_MAX_UDEG + 1}));
