#include "./imagery.hpp"
#include <bit>
#include <cstring>
#include <type_traits>

struct s_image_layer {
  t_imagery_header header;
  const uint8_t *grey;
} typedef t_image_layer;

// Sub-pixel position for the interpolation: 1/256 of a pixel
constexpr uint32_t FRACTION_BITS = 8;
constexpr int64_t FRACTION_ONE = int64_t{1} << FRACTION_BITS;
constexpr uint32_t CORNERS = 4;

static t_image_layer g_layers[IMAGERY_MAX_LAYERS];
static uint8_t g_layer_count = 0;

void imagery_clear() { g_layer_count = 0; }

static bool header_is_valid(const t_imagery_header &h) {
  return (h.magic == IMAGERY_MAGIC && h.version == IMAGERY_VERSION &&
          h.step_udeg != 0 && h.step_udeg <= UDEG_PER_DEG && h.width != 0 &&
          h.height != 0 && h.width <= IMAGERY_MAX_SIDE &&
          h.height <= IMAGERY_MAX_SIDE && h.west_udeg < LON_MAX_UDEG &&
          h.north_udeg >= -LAT_MAX_UDEG && h.north_udeg <= LAT_MAX_UDEG);
}

bool imagery_add_layer(const void *blob, uint32_t size) {
  if (!blob || reinterpret_cast<uintptr_t>(blob) % IMAGERY_ALIGN != 0 ||
      size < sizeof(t_imagery_header) || g_layer_count >= IMAGERY_MAX_LAYERS)
    return (false);
  t_image_layer layer = {};
  memcpy(&layer.header, blob, sizeof(layer.header));
  if (!header_is_valid(layer.header))
    return (false);
  const uint32_t pixels =
      uint32_t{layer.header.width} * layer.header.height;
  const uint32_t exact = sizeof(layer.header) + pixels;
  // Padded to 4 bytes by the tool
  if (size < exact || size >= exact + IMAGERY_ALIGN)
    return (false);
  layer.grey = static_cast<const uint8_t *>(blob) + sizeof(layer.header);
  uint8_t at = g_layer_count;
  while (at > 0 && g_layers[at - 1].header.step_udeg > layer.header.step_udeg) {
    g_layers[at] = g_layers[at - 1];
    at--;
  }
  g_layers[at] = layer;
  g_layer_count++;
  return (true);
}

uint8_t imagery_layer_count() { return (g_layer_count); }

const t_imagery_header *imagery_layer(uint8_t layer) {
  return (layer < g_layer_count ? &g_layers[layer].header : nullptr);
}

static uint8_t pixel(const t_image_layer &layer, int64_t col, int64_t row) {
  const t_imagery_header &h = layer.header;
  if (col < 0 || row < 0 || col >= h.width || row >= h.height)
    return (IMAGERY_NODATA);
  return (layer.grey[row * h.width + col]);
}

static bool layer_sample(const t_image_layer &layer, t_coord coord,
                         uint16_t *grey_x256) {
  const t_imagery_header &h = layer.header;
  if (coord.lon_udeg < h.west_udeg || coord.lat_udeg > h.north_udeg)
    return (false);
  const int64_t x =
      (int64_t{coord.lon_udeg} - h.west_udeg) * FRACTION_ONE / h.step_udeg;
  const int64_t y =
      (int64_t{h.north_udeg} - coord.lat_udeg) * FRACTION_ONE / h.step_udeg;
  const uint8_t centre = pixel(layer, x >> FRACTION_BITS, y >> FRACTION_BITS);
  if (centre == IMAGERY_NODATA)
    return (false);
  const int64_t fx = x - FRACTION_ONE / 2;
  const int64_t fy = y - FRACTION_ONE / 2;
  const int64_t tx = fx & (FRACTION_ONE - 1);
  const int64_t ty = fy & (FRACTION_ONE - 1);
  int64_t corner[CORNERS] = {};
  for (uint32_t i = 0; i < CORNERS; i++) {
    const uint8_t v = pixel(layer, (fx >> FRACTION_BITS) + (i & 1),
                            (fy >> FRACTION_BITS) + (i >> 1));
    corner[i] = v == IMAGERY_NODATA ? centre : v;
  }
  const int64_t top = corner[0] * (FRACTION_ONE - tx) + corner[1] * tx;
  const int64_t bottom = corner[2] * (FRACTION_ONE - tx) + corner[3] * tx;
  *grey_x256 = static_cast<uint16_t>(
      (top * (FRACTION_ONE - ty) + bottom * ty) / FRACTION_ONE);
  return (true);
}

bool imagery_sample(t_coord coord, uint16_t *grey_x256) {
  if (!grey_x256 || !coord_is_valid(coord))
    return (false);
  for (uint8_t i = 0; i < g_layer_count; i++)
    if (layer_sample(g_layers[i], coord, grey_x256))
      return (true);
  return (false);
}

static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(t_imagery_header) == 28, "header must have no padding");
static_assert(std::is_trivially_copyable_v<t_imagery_header>);
static_assert(int64_t{UINT8_MAX} * FRACTION_ONE <= UINT16_MAX,
              "a grey in 1/256 steps fits 16 bits");
static_assert(uint64_t{IMAGERY_MAX_SIDE} * IMAGERY_MAX_SIDE +
                  sizeof(t_imagery_header) + IMAGERY_ALIGN <
              UINT32_MAX);
static_assert(FRACTION_ONE == IMAGERY_GREY_ONE);
