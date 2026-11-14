#pragma once

#include "./coord.hpp"
#include <cstdint>

// Landform = static elevation + slope grid around the landing site, made by
// tools/fetch_landform.py from MOLA MEGDR 32 px/deg.
//
// The zone is a read-only blob that stays where it is (flash, memory mapped
// QSPI, mmap on a PC): nothing is copied, nothing is allocated.
//   blob (little endian, 4 bytes aligned):
//     t_landform_header                        20 bytes
//     int16_t  elevation_m[width * height]     row 0 = north
//     uint8_t  slope_half_deg[width * height]  precomputed, 0.5 deg steps

// Constants //
constexpr uint32_t LANDFORM_MAGIC = 0x414C4F4D; // "MOLA" as little endian bytes
constexpr uint16_t LANDFORM_VERSION = 2;
constexpr uint32_t LANDFORM_PX_PER_DEG = 32;
constexpr uint32_t LANDFORM_STEP_UDEG = UDEG_PER_DEG / LANDFORM_PX_PER_DEG;
// Blob alignment: Cortex-M0 faults on unaligned 16/32 bit reads
constexpr uint32_t LANDFORM_ALIGN = 4;
// MOLA min/max topography, in meters
constexpr int32_t MOLA_MIN_M = -8206;
constexpr int32_t MOLA_MAX_M = 21181;
// A 90 deg slope is 180 half degrees
constexpr uint32_t LANDFORM_MAX_SLOPE_HALF_DEG = 180;
constexpr uint16_t LANDFORM_MAX_SIDE = 4096;

// Structures //
struct s_landform_header {
  uint32_t magic;
  uint32_t west_udeg; // longitude of column 0 (west edge)
  int32_t north_udeg; // latitude of row 0 (north edge)
  uint16_t width;
  uint16_t height;
  uint16_t version;
  uint16_t reserved; // keeps the layout explicit, no hidden padding
} typedef t_landform_header;

struct s_landform {
  int16_t elevation_m;
  uint8_t slope_half_deg; // 0.5 deg steps, 255 = 127.5 deg or more
  bool valid;
} typedef t_landform;

// Functions //
// Checks the blob (size, magic, version, alignment) and starts using it.
// The blob must stay alive and unchanged while it is in use.
bool landform_init(const void *blob, uint32_t size);
t_landform get_landform(t_coord);
