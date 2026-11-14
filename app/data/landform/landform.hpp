#pragma once

#include "../core/coord.hpp"
#include <cstdint>

// Landform = static elevation + slope grids, in layers of different
// resolutions, mixed at read time: the finest layer that covers the point
// with data wins, the others fill the gaps.
//   CTX DTM mosaic (~20 m) around the landing site, where it exists
//   MOLA MEGDR 32 px/deg (~1.85 km) for the whole zone
// Both are tied to the MOLA areoid (the CTX products are controlled to MOLA
// topography), so their elevations agree. Blobs are made by
// tools/fetch_landform.py.
//
// A layer is a read-only blob that stays where it is (flash, mmap): nothing is
// copied, nothing is allocated.
//   blob (little endian, 4 bytes aligned):
//     t_landform_header                        24 bytes
//     int16_t  elevation_m[width * height]     row 0 = north, NODATA = hole
//     uint8_t  slope_half_deg[width * height]  precomputed, 0.5 deg steps
// Columns and rows are equal steps of degrees (equirectangular grid).

// Constants //
constexpr uint32_t LANDFORM_MAGIC = 0x414C4F4D; // "MOLA" as little endian bytes
constexpr uint16_t LANDFORM_VERSION = 3;        // v3: step in the header
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
// int16_t elevation + uint8_t slope
constexpr uint32_t LANDFORM_BYTES_PER_CELL = 3;
constexpr uint8_t LANDFORM_MAX_LAYERS = 4;

// Structures //
struct s_landform_header {
  uint32_t magic;
  uint32_t west_udeg;  // longitude of the west edge of column 0
  int32_t north_udeg;  // latitude of the north edge of row 0
  uint32_t step_udeg;  // cell size, both axes
  uint16_t width;
  uint16_t height;
  uint16_t version;
  uint16_t reserved; // keeps the layout explicit, no hidden padding
} typedef t_landform_header;

struct s_landform {
  int16_t elevation_m;
  uint8_t slope_half_deg; // 0.5 deg steps, 180 = 90 deg (vertical)
  uint8_t layer;          // index in landform_layer(), 0 = finest
  bool valid;
} typedef t_landform;

// Functions //
// Drops every layer.
void landform_clear();
// Checks the blob (size, magic, version, alignment, step) and adds it as a
// layer, kept sorted from the finest step to the coarsest. False if refused
// or if LANDFORM_MAX_LAYERS are already loaded. The blob must stay alive and
// unchanged while it is in use.
bool landform_add_layer(const void *blob, uint32_t size);
// landform_clear() + landform_add_layer(): a single zone
bool landform_init(const void *blob, uint32_t size);
uint8_t landform_layer_count();
// Header of a loaded layer, nullptr if none
const t_landform_header *landform_layer(uint8_t layer);
// Finest layer with data at `coord`
t_landform landform_get(t_coord coord);
