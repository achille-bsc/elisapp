#pragma once

#include <cstdint>

// Planetocentric coordinates in micro-degrees (1e-6 deg), MOLA convention:
//   lon_udeg = east longitude, [0, 360e6)
//   lat_udeg = latitude, [-90e6, 90e6]
// The GPS packet (app/core/gps) and the pathfinding GeoPoint (app/core/
// pathfinding) use 1e-7 deg ("E7"): convert at the border with coord_from_e7*.

// Constants //
constexpr uint32_t UDEG_PER_DEG = 1000000;
constexpr uint32_t LON_MAX_DEG = 360;
constexpr int32_t LAT_MAX_DEG = 90;
constexpr uint32_t LON_MAX_UDEG = LON_MAX_DEG * UDEG_PER_DEG;
constexpr uint32_t LON_HALF_UDEG = LON_MAX_UDEG / 2;
constexpr int32_t LAT_MAX_UDEG =
    LAT_MAX_DEG * static_cast<int32_t>(UDEG_PER_DEG);
constexpr int32_t E7_PER_UDEG = 10;

// Structures //
struct s_coord {
  uint32_t lon_udeg;
  int32_t lat_udeg;
} typedef t_coord;

// Functions //
constexpr bool coord_is_valid(t_coord coord) {
  return (coord.lon_udeg < LON_MAX_UDEG && coord.lat_udeg >= -LAT_MAX_UDEG &&
          coord.lat_udeg <= LAT_MAX_UDEG);
}

// E7 -> micro-degrees, rounded half away from zero
constexpr int32_t e7_to_udeg(int32_t e7) {
  const int32_t half = E7_PER_UDEG / 2;
  return ((e7 >= 0 ? e7 + half : e7 - half) / E7_PER_UDEG);
}

// Any longitude in micro-degrees (negative = west) -> [0, 360e6)
constexpr uint32_t lon_wrap_udeg(int64_t lon_udeg) {
  const int64_t lon_max = LON_MAX_UDEG;
  return (static_cast<uint32_t>(((lon_udeg % lon_max) + lon_max) % lon_max));
}

// Signed longitude in [-180, 180] deg (pathfinding GeoPoint)
constexpr t_coord coord_from_e7(int32_t lat_e7, int32_t lon_e7) {
  return (t_coord{lon_wrap_udeg(e7_to_udeg(lon_e7)), e7_to_udeg(lat_e7)});
}

// Unsigned east longitude in [0, 360) deg (GPS packet)
constexpr t_coord coord_from_e7_east(int32_t lat_e7, uint32_t lon_east_e7) {
  const uint32_t per_udeg = static_cast<uint32_t>(E7_PER_UDEG);
  return (t_coord{lon_wrap_udeg((lon_east_e7 + per_udeg / 2) / per_udeg),
                  e7_to_udeg(lat_e7)});
}

constexpr int32_t coord_lat_e7(t_coord coord) {
  return (coord.lat_udeg * E7_PER_UDEG);
}

// Back to a signed longitude in (-180, 180] deg, the pathfinding convention
constexpr int32_t coord_lon_e7(t_coord coord) {
  const int64_t lon = coord.lon_udeg > LON_HALF_UDEG
                          ? int64_t{coord.lon_udeg} - LON_MAX_UDEG
                          : int64_t{coord.lon_udeg};
  return (static_cast<int32_t>(lon * E7_PER_UDEG));
}

// Shortest signed difference b - a in micro-degrees, in (-180e6, 180e6]
constexpr int32_t lon_delta_udeg(uint32_t a, uint32_t b) {
  int64_t delta = int64_t{b} - int64_t{a};
  if (delta > LON_HALF_UDEG)
    delta -= LON_MAX_UDEG;
  else if (delta <= -int64_t{LON_HALF_UDEG})
    delta += LON_MAX_UDEG;
  return (static_cast<int32_t>(delta));
}

static_assert(LON_MAX_UDEG == 360000000u);
static_assert(sizeof(t_coord) == 8);
static_assert(coord_is_valid({0, 0}));
static_assert(coord_is_valid({LON_MAX_UDEG - 1, -LAT_MAX_UDEG}));
static_assert(!coord_is_valid({LON_MAX_UDEG, 0}));
static_assert(!coord_is_valid({0, LAT_MAX_UDEG + 1}));
// 180 deg in E7 must fit the int32 GeoPoint of the pathfinding
static_assert(int64_t{LON_HALF_UDEG} * E7_PER_UDEG <= INT32_MAX);
static_assert(int64_t{LAT_MAX_UDEG} * E7_PER_UDEG <= INT32_MAX);
static_assert(e7_to_udeg(15) == 2 && e7_to_udeg(-15) == -2);
static_assert(e7_to_udeg(14) == 1 && e7_to_udeg(-14) == -1);
static_assert(coord_from_e7(184446000, 774509000).lon_udeg == 77450900);
static_assert(coord_from_e7(0, -10).lon_udeg == LON_MAX_UDEG - 1);
static_assert(coord_from_e7_east(0, 3599999999u).lon_udeg == 0);
static_assert(coord_lon_e7({LON_MAX_UDEG - 1, 0}) == -10);
static_assert(coord_lon_e7(coord_from_e7(0, -1799999990)) == -1799999990);
static_assert(lon_delta_udeg(LON_MAX_UDEG - 1, 1) == 2);
static_assert(lon_delta_udeg(1, LON_MAX_UDEG - 1) == -2);
