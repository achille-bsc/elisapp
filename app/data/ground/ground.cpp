#include "./ground.hpp"
#include "../core/geodesy.hpp"
#include <cstring>

static t_ground_header g_header = {};
static const uint8_t *g_inertia = nullptr;

bool ground_set(const void *blob, uint32_t size) {
  g_inertia = nullptr;
  if (!blob)
    return (true);
  t_ground_header h = {};
  if (reinterpret_cast<uintptr_t>(blob) % GROUND_ALIGN != 0 || size < sizeof(h))
    return (false);
  memcpy(&h, blob, sizeof(h));
  const uint32_t exact = sizeof(h) + uint32_t{h.width} * h.height;
  if (h.magic != GROUND_MAGIC || h.version != GROUND_VERSION ||
      h.step_udeg == 0 || h.step_udeg > UDEG_PER_DEG || h.width == 0 ||
      h.height == 0 || h.width > GROUND_MAX_SIDE || h.height > GROUND_MAX_SIDE ||
      h.west_udeg >= LON_MAX_UDEG || h.north_udeg > LAT_MAX_UDEG ||
      h.north_udeg < -LAT_MAX_UDEG || size < exact ||
      size >= exact + GROUND_ALIGN)
    return (false);
  g_header = h;
  g_inertia = static_cast<const uint8_t *>(blob) + sizeof(h);
  return (true);
}

t_ground ground_get(t_coord coord) {
  const t_ground none = {};
  const t_ground_header &h = g_header;
  if (!g_inertia || !coord_is_valid(coord) || coord.lon_udeg < h.west_udeg ||
      coord.lat_udeg > h.north_udeg)
    return (none);
  const uint32_t col = (coord.lon_udeg - h.west_udeg) / h.step_udeg;
  const uint32_t row =
      static_cast<uint32_t>(h.north_udeg - coord.lat_udeg) / h.step_udeg;
  if (col >= h.width || row >= h.height)
    return (none);
  const uint8_t v = g_inertia[row * h.width + col];
  if (v == GROUND_NODATA)
    return (none);
  return (t_ground{static_cast<uint16_t>(v * GROUND_TIU_PER_STEP), true});
}

t_ground ground_softest(t_coord coord, uint32_t radius_mm) {
  t_ground best = ground_get(coord);
  if (!g_inertia || !coord_is_valid(coord))
    return (best);
  const t_ground_header &h = g_header;
  const uint32_t lon_q16 = mm_per_udeg_lon_q16(coord.lat_udeg);
  const auto pixels = [&h](uint64_t udeg) {
    const uint64_t n = udeg / h.step_udeg + 1;
    return (static_cast<int64_t>(n < GROUND_AREA_MAX_PIXELS ? n : GROUND_AREA_MAX_PIXELS));
  };
  const int64_t rows = pixels(uint64_t{radius_mm} * Q16_ONE / MM_PER_UDEG_LAT_Q16);
  const int64_t cols = pixels(lon_q16 ? uint64_t{radius_mm} * Q16_ONE / lon_q16 : 0);
  const int64_t col = (int64_t{coord.lon_udeg} - h.west_udeg) / h.step_udeg -
                      (coord.lon_udeg < h.west_udeg ? 1 : 0);
  const int64_t row = (int64_t{h.north_udeg} - coord.lat_udeg) / h.step_udeg -
                      (coord.lat_udeg > h.north_udeg ? 1 : 0);
  // Offset from the north-west corner, unwrapped like col (no layer crosses 0 E)
  const int64_t px = int64_t{coord.lon_udeg} - h.west_udeg;
  const int64_t py = int64_t{h.north_udeg} - coord.lat_udeg;
  const int64_t step = h.step_udeg;
  // Up to (2^32 - 1)^2: unsigned
  const uint64_t radius2 = uint64_t{radius_mm} * radius_mm;
  for (int64_t r = row - rows; r <= row + rows; r++)
    for (int64_t c = col - cols; c <= col + cols; c++) {
      if (r < 0 || c < 0 || r >= h.height || c >= h.width)
        continue;
      // Pixels whose nearest point is within the radius (the disc)
      const int64_t nx = px < c * step ? c * step : (px > (c + 1) * step ? (c + 1) * step : px);
      const int64_t ny = py < r * step ? r * step : (py > (r + 1) * step ? (r + 1) * step : py);
      const int64_t dx = (px - nx) * lon_q16 >> Q16_SHIFT;
      const int64_t dy = (py - ny) * int64_t{MM_PER_UDEG_LAT_Q16} >> Q16_SHIFT;
      if (static_cast<uint64_t>(dx * dx + dy * dy) > radius2)
        continue;
      const uint8_t v = g_inertia[r * h.width + c];
      const uint16_t tiu = static_cast<uint16_t>(v * GROUND_TIU_PER_STEP);
      if (v != GROUND_NODATA && (!best.valid || tiu < best.inertia_tiu))
        best = t_ground{tiu, true};
    }
  return (best);
}

static_assert(sizeof(t_ground_header) == 28, "no hidden padding");
static_assert(uint32_t{UINT8_MAX} * GROUND_TIU_PER_STEP <= UINT16_MAX);
static_assert(uint64_t{GROUND_MAX_SIDE} * GROUND_MAX_SIDE + 28 + GROUND_ALIGN <
              UINT32_MAX);
// ground_softest: a pixel offset (at most GROUND_AREA_MAX_PIXELS + 1 pixels
// of a degree) in mm, squared twice, stays in int64
static_assert(int64_t{(GROUND_AREA_MAX_PIXELS + 1) * UDEG_PER_DEG} *
                      (MM_PER_UDEG_LAT_Q16 >> Q16_SHIFT) *
                      ((GROUND_AREA_MAX_PIXELS + 1) * UDEG_PER_DEG) *
                      (MM_PER_UDEG_LAT_Q16 >> Q16_SHIFT) <
              INT64_MAX / 4);
