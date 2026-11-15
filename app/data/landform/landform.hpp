#pragma once

#include "../core/coord.hpp"
#include <cstdint>

// Elevation and slope in layers (HiRISE 2 m ... MOLA 1.85 km), all on the
// MOLA areoid: the finest layer with data wins. Read-only blobs read in place.
//   blob (little endian, 4 bytes aligned):
//     t_landform_header                        28 bytes
//     int16_t  elevation[width * height]       row 0 = north, NODATA = hole
//              metres = base_m + elevation * unit_mm / 1000
//     uint8_t  slope_half_deg[width * height]  precomputed, 0.5 deg steps
//     uint8_t  near_half_deg[width * height]   steepest slope within 50 m

// Constants //
constexpr uint32_t LANDFORM_MAGIC = 0x414C4F4D; // "MOLA" as little endian bytes
constexpr uint16_t LANDFORM_VERSION = 5; // v5: clearance slope
// unit_mm = 0 in a header: whole metres
constexpr uint16_t LANDFORM_UNIT_MM_DEFAULT = 1000;
constexpr int32_t MM_PER_M_I = 1000;
// Header limits, so every stored value in mm fits 32 bits:
// |base| + 32767 * unit stays within +-65.6 km, far beyond Mars' relief
constexpr uint16_t LANDFORM_MAX_UNIT_MM = 1000;
constexpr int32_t LANDFORM_MAX_BASE_M = 32767;
constexpr uint32_t LANDFORM_MOLA_PX_PER_DEG = 32;
constexpr uint32_t LANDFORM_MOLA_STEP_UDEG =
    UDEG_PER_DEG / LANDFORM_MOLA_PX_PER_DEG;
// Coarsest accepted step: 1 deg
constexpr uint32_t LANDFORM_MAX_STEP_UDEG = UDEG_PER_DEG;
// Blob alignment: some ARM cores fault on unaligned 16/32 bit reads
constexpr uint32_t LANDFORM_ALIGN = 4;
// MOLA min/max topography, in meters
constexpr int32_t MOLA_MIN_M = -8206;
constexpr int32_t MOLA_MAX_M = 21181;
// A hole in a layer (outside the CTX footprint): read the next layer
constexpr int16_t LANDFORM_NODATA_M = INT16_MIN;
// A 90 deg slope is 180 half degrees
constexpr uint32_t LANDFORM_MAX_SLOPE_HALF_DEG = 180;
// 16384 cells of 20 m = 330 km per side, 805 MB: still 32 bit maths
constexpr uint16_t LANDFORM_MAX_SIDE = 16384;
// int16_t elevation + uint8_t slope + uint8_t clearance slope
constexpr uint32_t LANDFORM_BYTES_PER_CELL = 4;
constexpr uint32_t LANDFORM_CLEARANCE_M = 50;
constexpr uint8_t LANDFORM_MAX_LAYERS = 4;
// landform_steepest() reads at most this many cells each way from the centre
constexpr uint32_t LANDFORM_AREA_MAX_CELLS = 8;

// Structures //
struct s_landform_header {
  uint32_t magic;
  uint32_t west_udeg;  // longitude of the west edge of column 0
  int32_t north_udeg;  // latitude of the north edge of row 0
  uint32_t step_udeg;  // cell size, both axes
  uint16_t width;
  uint16_t height;
  uint16_t version;
  uint16_t unit_mm; // elevation step, 0 = LANDFORM_UNIT_MM_DEFAULT
  int32_t base_m;   // elevation of the stored value 0
} typedef t_landform_header;

struct s_landform {
  int16_t elevation_m;
  uint8_t slope_half_deg; // 0.5 deg steps, 180 = 90 deg (vertical)
  uint8_t layer;          // index in landform_layer(), 0 = finest
  bool valid;
  uint8_t near_half_deg;  // steepest slope within LANDFORM_CLEARANCE_M
} typedef t_landform;

// Functions //
void landform_clear();
// False if refused or full. The blob must stay alive and unchanged while used.
bool landform_add_layer(const void *blob, uint32_t size);
// landform_clear() + landform_add_layer(): a single zone
bool landform_init(const void *blob, uint32_t size);
uint8_t landform_layer_count();
// Header of a loaded layer, nullptr if none
const t_landform_header *landform_layer(uint8_t layer);
// Finest layer with data at `coord`
t_landform landform_get(t_coord coord);
// Interpolated elevation in mm, for drawing (hazards use landform_get)
bool landform_sample_mm(t_coord coord, int32_t *elevation_mm);
// landform_get() with the steepest slope within `radius_mm`
t_landform landform_steepest(t_coord coord, uint32_t radius_mm);
