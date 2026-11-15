#pragma once

#include "../core/coord.hpp"
#include <cstdint>

// Ground thermal inertia: low inertia = dust or loose fines (soft ground).
// TES (Putzig & Mellon 2007) refined to 100 m with THEMIS night IR.
//   blob (little endian, 4 bytes aligned, read in place):
//     t_ground_header                          28 bytes (same as imagery)
//     uint8_t inertia[width * height]          * GROUND_TIU_PER_STEP, 0 = none
//     up to 3 bytes of padding

// Constants //
constexpr uint32_t GROUND_MAGIC = 0x524E4954; // "TINR" as little endian
constexpr uint16_t GROUND_VERSION = 1;
constexpr uint8_t GROUND_NODATA = 0;
constexpr uint16_t GROUND_TIU_PER_STEP = 8; // tiu = J m-2 K-1 s-1/2
constexpr uint16_t GROUND_MAX_SIDE = 16384;
constexpr uint32_t GROUND_ALIGN = 4;
constexpr uint32_t GROUND_AREA_MAX_PIXELS = 4;

// Structures //
struct s_ground_header {
  uint32_t magic;
  uint32_t west_udeg;
  int32_t north_udeg;
  uint32_t step_udeg;
  uint16_t width;
  uint16_t height;
  uint16_t version;
  uint16_t reserved;
  int32_t reserved2;
} typedef t_ground_header;

struct s_ground {
  uint16_t inertia_tiu;
  bool valid;
} typedef t_ground;

// Functions //
// Keeps the blob (nullptr: none); it must stay alive while used.
bool ground_set(const void *blob, uint32_t size);
t_ground ground_get(t_coord coord);
// Lowest inertia within `radius_mm` of `coord`
t_ground ground_softest(t_coord coord, uint32_t radius_mm);
