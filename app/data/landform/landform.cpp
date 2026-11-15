#include "./landform.hpp"
#include "../core/geodesy.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>

struct s_layer {
  t_landform_header header;
  const int16_t *elevation;
  const uint8_t *slope;
  const uint8_t *near;
  uint16_t unit_mm; // never 0
} typedef t_layer;

// Sub-cell position for the interpolation: 1/256 of a cell
constexpr uint32_t FRACTION_BITS = 8;
constexpr int64_t FRACTION_ONE = int64_t{1} << FRACTION_BITS;

// A few headers and pointers of RAM: the grids are read in place
static t_layer g_layers[LANDFORM_MAX_LAYERS];
static uint8_t g_layer_count = 0;

void landform_clear() { g_layer_count = 0; }

static bool header_is_valid(const t_landform_header &h) {
  return (h.magic == LANDFORM_MAGIC && h.version == LANDFORM_VERSION &&
          h.step_udeg != 0 && h.step_udeg <= LANDFORM_MAX_STEP_UDEG &&
          h.width != 0 && h.height != 0 && h.width <= LANDFORM_MAX_SIDE &&
          h.height <= LANDFORM_MAX_SIDE && h.west_udeg < LON_MAX_UDEG &&
          h.north_udeg >= -LAT_MAX_UDEG && h.north_udeg <= LAT_MAX_UDEG &&
          h.unit_mm <= LANDFORM_MAX_UNIT_MM &&
          h.base_m >= -LANDFORM_MAX_BASE_M && h.base_m <= LANDFORM_MAX_BASE_M);
}

bool landform_add_layer(const void *blob, uint32_t size) {
  if (!blob || reinterpret_cast<uintptr_t>(blob) % LANDFORM_ALIGN != 0 ||
      size < sizeof(t_landform_header) || g_layer_count >= LANDFORM_MAX_LAYERS)
    return (false);
  t_layer layer = {};
  memcpy(&layer.header, blob, sizeof(layer.header));
  if (!header_is_valid(layer.header))
    return (false);
  const uint32_t count =
      static_cast<uint32_t>(layer.header.width) * layer.header.height;
  // Exact size: refuses truncated or padded blobs
  if (size != sizeof(layer.header) + count * LANDFORM_BYTES_PER_CELL)
    return (false);
  const uint8_t *bytes = static_cast<const uint8_t *>(blob);
  layer.elevation =
      reinterpret_cast<const int16_t *>(bytes + sizeof(layer.header));
  layer.slope = bytes + sizeof(layer.header) + count * sizeof(int16_t);
  layer.near = layer.slope + count;
  layer.unit_mm = layer.header.unit_mm ? layer.header.unit_mm
                                       : LANDFORM_UNIT_MM_DEFAULT;

  // Insertion sort on the step: index 0 stays the finest layer
  uint8_t at = g_layer_count;
  while (at > 0 && g_layers[at - 1].header.step_udeg > layer.header.step_udeg) {
    g_layers[at] = g_layers[at - 1];
    at--;
  }
  g_layers[at] = layer;
  g_layer_count++;
  return (true);
}

bool landform_init(const void *blob, uint32_t size) {
  landform_clear();
  return (landform_add_layer(blob, size));
}

uint8_t landform_layer_count() { return (g_layer_count); }

const t_landform_header *landform_layer(uint8_t layer) {
  return (layer < g_layer_count ? &g_layers[layer].header : nullptr);
}

static int64_t elevation_mm_of(const t_layer &layer, int16_t raw) {
  return (int64_t{layer.header.base_m} * MM_PER_M_I +
          int64_t{raw} * layer.unit_mm);
}

static int16_t to_metres(int64_t mm) {
  const int64_t half = MM_PER_M_I / 2;
  int64_t m = (mm >= 0 ? mm + half : mm - half) / MM_PER_M_I;
  // NODATA stays reserved for holes
  if (m <= LANDFORM_NODATA_M)
    m = LANDFORM_NODATA_M + 1;
  if (m > INT16_MAX)
    m = INT16_MAX;
  return (static_cast<int16_t>(m));
}

// Raw post (col, row) of a layer, false outside it or in a hole
static bool layer_post(const t_layer &layer, int64_t col, int64_t row,
                       int16_t *raw) {
  const t_landform_header &h = layer.header;
  if (col < 0 || row < 0 || col >= h.width || row >= h.height)
    return (false);
  const int16_t v = layer.elevation[row * h.width + col];
  *raw = v;
  return (v != LANDFORM_NODATA_M);
}

// Position of `coord` in a layer, in 1/256 cells from its north-west corner.
// False west or north of the layer.
static bool layer_position(const t_landform_header &h, t_coord coord,
                           int64_t *x, int64_t *y) {
  // Below the west/north edge the unsigned maths would wrap: check first
  if (coord.lon_udeg < h.west_udeg || coord.lat_udeg > h.north_udeg)
    return (false);
  *x = (int64_t{coord.lon_udeg} - h.west_udeg) * FRACTION_ONE / h.step_udeg;
  *y = (int64_t{h.north_udeg} - coord.lat_udeg) * FRACTION_ONE / h.step_udeg;
  return (true);
}

// Cell of one layer, valid == false outside it or in a hole
static t_landform layer_get(const t_layer &layer, uint8_t index,
                            t_coord coord) {
  const t_landform none = {};
  int64_t x = 0;
  int64_t y = 0;
  int16_t raw = 0;
  if (!layer_position(layer.header, coord, &x, &y) ||
      !layer_post(layer, x >> FRACTION_BITS, y >> FRACTION_BITS, &raw))
    return (none);
  const uint32_t cell = static_cast<uint32_t>(y >> FRACTION_BITS) *
                            layer.header.width +
                        static_cast<uint32_t>(x >> FRACTION_BITS);
  return (t_landform{to_metres(elevation_mm_of(layer, raw)), layer.slope[cell],
                     index, true, layer.near[cell]});
}

t_landform landform_get(t_coord coord) {
  if (coord_is_valid(coord))
    for (uint8_t i = 0; i < g_layer_count; i++) {
      const t_landform cell = layer_get(g_layers[i], i, coord);
      if (cell.valid)
        return (cell);
    }
  return (t_landform{});
}

t_landform landform_steepest(t_coord coord, uint32_t radius_mm) {
  t_landform here = landform_get(coord);
  if (!here.valid)
    return (here);
  const t_layer &layer = g_layers[here.layer];
  const t_landform_header &h = layer.header;
  // Radius in cells along each axis, capped
  const uint64_t lat_udeg = uint64_t{radius_mm} * Q16_ONE / MM_PER_UDEG_LAT_Q16;
  const uint32_t lon_mm_q16 = mm_per_udeg_lon_q16(coord.lat_udeg);
  const uint64_t lon_udeg =
      lon_mm_q16 ? uint64_t{radius_mm} * Q16_ONE / lon_mm_q16 : lat_udeg;
  const auto cells = [&h](uint64_t udeg) {
    const uint64_t n = udeg / h.step_udeg;
    return (static_cast<int64_t>(n < LANDFORM_AREA_MAX_CELLS ? n : LANDFORM_AREA_MAX_CELLS));
  };
  const int64_t rows = cells(lat_udeg);
  const int64_t cols = cells(lon_udeg);
  int64_t x = 0;
  int64_t y = 0;
  layer_position(h, coord, &x, &y);
  const int64_t col = x >> FRACTION_BITS;
  const int64_t row = y >> FRACTION_BITS;
  for (int64_t r = row - rows; r <= row + rows; r++)
    for (int64_t c = col - cols; c <= col + cols; c++) {
      int16_t raw = 0;
      if (!layer_post(layer, c, r, &raw))
        continue;
      const uint8_t slope = layer.slope[r * h.width + c];
      if (slope > here.slope_half_deg)
        here.slope_half_deg = slope;
    }
  return (here);
}

bool landform_sample_mm(t_coord coord, int32_t *elevation_mm) {
  const t_landform here = landform_get(coord);
  if (!here.valid || !elevation_mm)
    return (false);
  const t_layer &layer = g_layers[here.layer];
  int64_t x = 0;
  int64_t y = 0;
  layer_position(layer.header, coord, &x, &y);
  // From the centre of the north-west post of the four around the point
  x -= FRACTION_ONE / 2;
  y -= FRACTION_ONE / 2;
  const int64_t col = x >> FRACTION_BITS; // floor, also below 0
  const int64_t row = y >> FRACTION_BITS;
  const int64_t tx = x & (FRACTION_ONE - 1);
  const int64_t ty = y & (FRACTION_ONE - 1);
  int16_t centre = 0;
  layer_post(layer, (x + FRACTION_ONE / 2) >> FRACTION_BITS,
             (y + FRACTION_ONE / 2) >> FRACTION_BITS, &centre);
  int64_t post[4] = {};
  for (uint32_t i = 0; i < 4; i++) {
    int16_t raw = 0;
    if (!layer_post(layer, col + (i & 1), row + (i >> 1), &raw))
      raw = centre;
    post[i] = raw;
  }
  const int64_t top = post[0] * (FRACTION_ONE - tx) + post[1] * tx;
  const int64_t bottom = post[2] * (FRACTION_ONE - tx) + post[3] * tx;
  const int64_t raw_q16 = top * (FRACTION_ONE - ty) + bottom * ty;
  const int64_t mm = int64_t{layer.header.base_m} * MM_PER_M_I +
                     raw_q16 * layer.unit_mm / (FRACTION_ONE * FRACTION_ONE);
  *elevation_mm = static_cast<int32_t>(mm);
  return (true);
}

static_assert(std::endian::native == std::endian::little,
              "zone blobs are little endian, add a byte swap for this target");
static_assert(sizeof(t_landform_header) == 28, "header must have no padding");
static_assert(sizeof(t_landform_header) % alignof(int16_t) == 0,
              "elevations follow the header and must stay 2 bytes aligned");
static_assert(std::is_trivially_copyable_v<t_landform_header>);
static_assert(sizeof(t_landform) == 6, "6 bytes handed to the front, no more");
static_assert(LANDFORM_MOLA_PX_PER_DEG * LANDFORM_MOLA_STEP_UDEG ==
                  UDEG_PER_DEG,
              "MOLA step is 1/32 deg");
static_assert(LON_MAX_UDEG % LANDFORM_MOLA_STEP_UDEG == 0,
              "a whole number of MOLA cells wraps the planet");
static_assert(std::numeric_limits<int16_t>::min() < MOLA_MIN_M,
              "NODATA stays below any real elevation");
static_assert(std::numeric_limits<int16_t>::max() >= MOLA_MAX_M);
// width * height and the blob size must fit the 32 bit maths of add_layer
static_assert(static_cast<uint64_t>(LANDFORM_MAX_SIDE) * LANDFORM_MAX_SIDE *
                      LANDFORM_BYTES_PER_CELL +
                  sizeof(t_landform_header) <
              std::numeric_limits<uint32_t>::max());
static_assert(LANDFORM_BYTES_PER_CELL == sizeof(int16_t) + 2 * sizeof(uint8_t));
static_assert(LANDFORM_MAX_LAYERS <= UINT8_MAX);
// Interpolation: raw (16 bits) * 2^16 * unit (16 bits) stays in 64 bits
static_assert(int64_t{INT16_MAX} * FRACTION_ONE * FRACTION_ONE * UINT16_MAX <
              INT64_MAX / 2);
// Any value a valid header allows, in mm, fits the int32 output
static_assert(int64_t{LANDFORM_MAX_BASE_M} * MM_PER_M_I +
                  int64_t{INT16_MAX} * LANDFORM_MAX_UNIT_MM <
              INT32_MAX);
static_assert(LANDFORM_UNIT_MM_DEFAULT <= LANDFORM_MAX_UNIT_MM);
// Any elevation of Mars in mm fits the int32 output
static_assert(int64_t{MOLA_MAX_M} * MM_PER_M_I < INT32_MAX);
// It must fit the uint8_t layer
static_assert(LANDFORM_MAX_SLOPE_HALF_DEG <=
              std::numeric_limits<uint8_t>::max());
