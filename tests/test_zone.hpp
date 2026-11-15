#pragma once

#include "data/data.hpp"
#include <cmath>
#include <cstring>
#include <vector>

// Relief layers and positions for the application tests.

// Structures //
typedef uint8_t (*t_slope_of)(uint32_t row, uint32_t col);

// Functions //
// Layer blob of the header's size, flat at `elevation_m`; `slope_of` gives
// each cell's slope (nullptr: 0 everywhere). 4 bytes aligned like a flash.
inline std::vector<uint32_t> test_zone_make(t_landform_header header,
                                            int16_t elevation_m,
                                            t_slope_of slope_of, uint32_t *size) {
  const uint32_t count = uint32_t{header.width} * header.height;
  *size = sizeof(header) + count * LANDFORM_BYTES_PER_CELL;
  std::vector<uint32_t> blob((*size + sizeof(uint32_t) - 1) / sizeof(uint32_t));
  uint8_t *const bytes = reinterpret_cast<uint8_t *>(blob.data());
  memcpy(bytes, &header, sizeof(header));
  int16_t *const elevation = reinterpret_cast<int16_t *>(bytes + sizeof(header));
  uint8_t *const slope = bytes + sizeof(header) + count * sizeof(int16_t);
  for (uint32_t i = 0; i < count; i++) {
    elevation[i] = elevation_m;
    slope[i] = slope_of ? slope_of(i / header.width, i % header.width) : 0;
  }
  // Clearance slope: the steepest slope of the cells within
  // LANDFORM_CLEARANCE_M (a square, like tools/fetch_site.py steepest_around;
  // a cell as wide as the clearance or wider covers it alone)
  uint8_t *const near = slope + count;
  const double step_mm = static_cast<double>(header.step_udeg) * MM_PER_UDEG_LAT_Q16 / Q16_ONE;
  const double clearance_mm = LANDFORM_CLEARANCE_M * 1000.0;
  const int32_t rows = step_mm >= clearance_mm ? 0 : static_cast<int32_t>(std::ceil(clearance_mm / step_mm));
  // The narrowest cells east-west: the side nearest the pole
  const int32_t pole_lat = header.north_udeg >= 0
                               ? header.north_udeg
                               : header.north_udeg - static_cast<int32_t>(header.height * header.step_udeg);
  const double lon_mm = static_cast<double>(header.step_udeg) * mm_per_udeg_lon_q16(pole_lat) / Q16_ONE;
  const int32_t cols = rows == 0 ? 0 : static_cast<int32_t>(std::ceil(clearance_mm / (lon_mm > 1.0 ? lon_mm : 1.0)));
  for (uint32_t i = 0; i < count; i++) {
    const int32_t row = static_cast<int32_t>(i / header.width);
    const int32_t col = static_cast<int32_t>(i % header.width);
    uint8_t most = 0;
    for (int32_t r = row - rows; r <= row + rows; r++)
      for (int32_t c = col - cols; c <= col + cols; c++)
        if (r >= 0 && c >= 0 && r < header.height && c < header.width) {
          const uint8_t v = slope[static_cast<uint32_t>(r) * header.width + static_cast<uint32_t>(c)];
          most = v > most ? v : most;
        }
    near[i] = most;
  }
  return (blob);
}

// `origin` moved by metres east and north (local flat projection)
inline mars::t_position test_offset(mars::t_position origin, double east_m,
                                    double north_m) {
  constexpr double MM_PER_M = 1000.0;
  const double lat_udeg = north_m * MM_PER_M * Q16_ONE / MM_PER_UDEG_LAT_Q16;
  const double lon_udeg = east_m * MM_PER_M * Q16_ONE /
                          mm_per_udeg_lon_q16(origin.latitudeE7 / E7_PER_UDEG);
  // West is negative: signed maths, then back into [0, 360) degrees
  constexpr int64_t FULL_TURN_E7 = int64_t{LON_MAX_UDEG} * E7_PER_UDEG;
  const int64_t lon = (int64_t{origin.longitudeEastE7} +
                       static_cast<int64_t>(lon_udeg * E7_PER_UDEG) + FULL_TURN_E7) %
                      FULL_TURN_E7;
  return (mars::t_position{
      origin.latitudeE7 + static_cast<int32_t>(lat_udeg * E7_PER_UDEG),
      static_cast<uint32_t>(lon)});
}

// Distance in metres between two positions
inline double test_distance_m(mars::t_position a, mars::t_position b) {
  constexpr double MM_PER_M = 1000.0;
  return (coord_distance_mm(coord_from_e7_east(a.latitudeE7, a.longitudeEastE7),
                            coord_from_e7_east(b.latitudeE7, b.longitudeEastE7)) /
          MM_PER_M);
}
