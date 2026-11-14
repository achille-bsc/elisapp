#pragma once

#include "../config.hpp"
#include "./coord.hpp"
#include "./fixed_math.hpp"
#include <cstdint>

// Local equirectangular maths on the Mars sphere, the same as the pathfinding
// LocalProjection: accurate to ~0.1 % over a few hundred kilometres.

// Constants //
// Millimetres per micro-degree of latitude: R * pi / 180 / 1e6, Q16
inline constexpr uint32_t MM_PER_UDEG_LAT_Q16 = static_cast<uint32_t>(
    ce_round(static_cast<double>(MARS_RADIUS_MM) * PI / DEG_PER_HALF_TURN /
             UDEG_PER_DEG * Q16_ONE));

// Functions //
// Millimetres per micro-degree of longitude at a latitude, Q16 (0 at a pole)
constexpr uint32_t mm_per_udeg_lon_q16(int32_t lat_udeg) {
  const uint32_t abs_lat =
      static_cast<uint32_t>(lat_udeg < 0 ? -int64_t{lat_udeg} : lat_udeg);
  return (static_cast<uint32_t>(uint64_t{MM_PER_UDEG_LAT_Q16} *
                                    static_cast<uint64_t>(cos_q30(abs_lat)) >>
                                Q30_SHIFT));
}

// Distance in millimetres, saturated at UINT32_MAX (~4295 km)
constexpr uint32_t coord_distance_mm(t_coord a, t_coord b) {
  const int32_t mid_lat =
      static_cast<int32_t>((int64_t{a.lat_udeg} + b.lat_udeg) / 2);
  const int64_t x = int64_t{lon_delta_udeg(a.lon_udeg, b.lon_udeg)} *
                    mm_per_udeg_lon_q16(mid_lat) >> Q16_SHIFT;
  const int64_t y =
      (int64_t{b.lat_udeg} - a.lat_udeg) * MM_PER_UDEG_LAT_Q16 >> Q16_SHIFT;
  // Checked before squaring: x * x + y * y must stay below 2^63
  if (x > INT32_MAX || -x > INT32_MAX || y > INT32_MAX || -y > INT32_MAX)
    return (UINT32_MAX);
  const uint64_t d = isqrt_u64(static_cast<uint64_t>(x * x + y * y));
  return (static_cast<uint32_t>(d > UINT32_MAX ? UINT32_MAX : d));
}

// ~59.16 mm per micro-degree on Mars
static_assert(MM_PER_UDEG_LAT_Q16 / Q16_ONE == 59);
static_assert(mm_per_udeg_lon_q16(LAT_MAX_UDEG) == 0);
static_assert(mm_per_udeg_lon_q16(0) == MM_PER_UDEG_LAT_Q16);
// 1 deg of latitude ~59.16 km
static_assert(coord_distance_mm({0, 0}, {0, 1000000}) / 1000000 == 59);
static_assert(coord_distance_mm({0, -LAT_MAX_UDEG}, {LON_HALF_UDEG, LAT_MAX_UDEG}) ==
              UINT32_MAX);
