#pragma once

#include "../core/coord.hpp"
#include <cstdint>

// Orbital images in layers (CTX 6 m, THEMIS 100 m), finest with data wins.
// For the map picture only.
//   blob (little endian, 4 bytes aligned, read in place):
//     t_imagery_header                         28 bytes
//     uint8_t grey[width * height]             row 0 = north, 0 = no data
//     up to 3 bytes of padding

// Constants //
constexpr uint32_t IMAGERY_MAGIC = 0x52474D49; // "IMGR" as little endian
constexpr uint16_t IMAGERY_VERSION = 1;
constexpr uint8_t IMAGERY_NODATA = 0;
constexpr uint8_t IMAGERY_MAX_LAYERS = 4;
constexpr uint32_t IMAGERY_ALIGN = 4;
constexpr uint16_t IMAGERY_MAX_SIDE = 16384;
// Grey of the interpolated sample, 1/256 steps: 255 * 256 at most
constexpr uint32_t IMAGERY_GREY_ONE = 256;

// Structures //
// Same layout as t_landform_header
struct s_imagery_header {
  uint32_t magic;
  uint32_t west_udeg; // longitude of the west edge of column 0
  int32_t north_udeg; // latitude of the north edge of row 0
  uint32_t step_udeg; // pixel size, both axes
  uint16_t width;
  uint16_t height;
  uint16_t version;
  uint16_t reserved;
  int32_t reserved2;
} typedef t_imagery_header;

// Functions //
void imagery_clear();
// The blob must stay alive and unchanged while in use.
bool imagery_add_layer(const void *blob, uint32_t size);
uint8_t imagery_layer_count();
const t_imagery_header *imagery_layer(uint8_t layer);
// Interpolated grey in 1/256 steps, false where no layer has data
bool imagery_sample(t_coord coord, uint16_t *grey_x256);
