#include "./landform.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>

struct s_layer {
  t_landform_header header;
  const int16_t *elevation;
  const uint8_t *slope;
} typedef t_layer;

// A few headers and pointers of RAM: the grids are read in place
static t_layer g_layers[LANDFORM_MAX_LAYERS];
static uint8_t g_layer_count = 0;

void landform_clear() { g_layer_count = 0; }

static bool header_is_valid(const t_landform_header &h) {
  return (h.magic == LANDFORM_MAGIC && h.version == LANDFORM_VERSION &&
          h.step_udeg != 0 && h.step_udeg <= LANDFORM_MAX_STEP_UDEG &&
          h.width != 0 && h.height != 0 && h.width <= LANDFORM_MAX_SIDE &&
          h.height <= LANDFORM_MAX_SIDE && h.west_udeg < LON_MAX_UDEG &&
          h.north_udeg >= -LAT_MAX_UDEG && h.north_udeg <= LAT_MAX_UDEG);
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

// Cell of one layer, valid == false outside it or in a hole
static t_landform layer_get(const t_layer &layer, uint8_t index,
                            t_coord coord) {
  const t_landform none = {};
  const t_landform_header &h = layer.header;
  // Below the west/north edge the unsigned maths would wrap: check first
  if (coord.lon_udeg < h.west_udeg || coord.lat_udeg > h.north_udeg)
    return (none);
  // Both differences fit 32 bits: lon in [0, 360e6), north - lat in [0, 180e6]
  const uint32_t col = (coord.lon_udeg - h.west_udeg) / h.step_udeg;
  const uint32_t row =
      static_cast<uint32_t>(h.north_udeg - coord.lat_udeg) / h.step_udeg;
  if (col >= h.width || row >= h.height)
    return (none);
  const uint32_t cell = row * h.width + col;
  if (layer.elevation[cell] == LANDFORM_NODATA_M)
    return (none);
  return (t_landform{layer.elevation[cell], layer.slope[cell], index, true});
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

static_assert(std::endian::native == std::endian::little,
              "zone blobs are little endian, add a byte swap for this target");
static_assert(sizeof(t_landform_header) == 24, "header must have no padding");
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
static_assert(LANDFORM_BYTES_PER_CELL == sizeof(int16_t) + sizeof(uint8_t));
static_assert(LANDFORM_MAX_LAYERS <= UINT8_MAX);
// It must fit the uint8_t layer
static_assert(LANDFORM_MAX_SLOPE_HALF_DEG <=
              std::numeric_limits<uint8_t>::max());
