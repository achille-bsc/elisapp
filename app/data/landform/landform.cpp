#include "./landform.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>

// Only 20 bytes of RAM + two pointers: the grids are read in place
static t_landform_header g_header;
static const int16_t *g_elevation = nullptr;
static const uint8_t *g_slope = nullptr;

bool landform_init(const void *blob, uint32_t size) {
  g_elevation = nullptr;
  g_slope = nullptr;
  if (!blob || reinterpret_cast<uintptr_t>(blob) % LANDFORM_ALIGN != 0 ||
      size < sizeof(t_landform_header))
    return (false);

  t_landform_header header;
  memcpy(&header, blob, sizeof(header));
  if (header.magic != LANDFORM_MAGIC || header.version != LANDFORM_VERSION ||
      header.width == 0 || header.height == 0 ||
      header.width > LANDFORM_MAX_SIDE || header.height > LANDFORM_MAX_SIDE ||
      header.west_udeg >= LON_MAX_UDEG || header.north_udeg < -LAT_MAX_UDEG ||
      header.north_udeg > LAT_MAX_UDEG)
    return (false);
  const uint32_t count = static_cast<uint32_t>(header.width) * header.height;
  // Exact size: refuses truncated or padded blobs
  if (size != sizeof(header) + count * (sizeof(int16_t) + sizeof(uint8_t)))
    return (false);

  const uint8_t *bytes = static_cast<const uint8_t *>(blob);
  g_header = header;
  g_elevation = reinterpret_cast<const int16_t *>(bytes + sizeof(header));
  g_slope = bytes + sizeof(header) + count * sizeof(int16_t);
  return (true);
}

t_landform get_landform(t_coord coord) {
  const t_landform none = {0, 0, false};
  // Below the west/north edge the unsigned maths would wrap: check first
  if (!g_elevation || !coord_is_valid(coord) || coord.x < g_header.west_udeg ||
      coord.y > g_header.north_udeg)
    return (none);
  // Both differences fit 32 bits: x in [0, 360e6), north - y in [0, 180e6]
  const uint32_t col = (coord.x - g_header.west_udeg) / LANDFORM_STEP_UDEG;
  const uint32_t row =
      static_cast<uint32_t>(g_header.north_udeg - coord.y) / LANDFORM_STEP_UDEG;
  if (col >= g_header.width || row >= g_header.height)
    return (none);
  const uint32_t index = row * g_header.width + col;
  return (t_landform{g_elevation[index], g_slope[index], true});
}

static_assert(std::endian::native == std::endian::little,
              "zone blobs are little endian, add a byte swap for this target");
static_assert(sizeof(t_landform_header) == 20, "header must have no padding");
static_assert(sizeof(t_landform_header) % alignof(int16_t) == 0,
              "elevations follow the header and must stay 2 bytes aligned");
static_assert(std::is_trivially_copyable_v<t_landform_header>);
static_assert(sizeof(t_landform) == 4, "4 bytes handed to the front, no more");
static_assert(LON_MAX_UDEG % LANDFORM_STEP_UDEG == 0,
              "a whole number of cells must wrap the planet");
static_assert(LANDFORM_PX_PER_DEG * LANDFORM_STEP_UDEG == UDEG_PER_DEG,
              "step is 1/LANDFORM_PX_PER_DEG deg");
static_assert(std::numeric_limits<int16_t>::min() <= MOLA_MIN_M);
static_assert(std::numeric_limits<int16_t>::max() >= MOLA_MAX_M);
// width * height and the blob size must fit the 32 bit maths in landform_init
static_assert(static_cast<uint64_t>(LANDFORM_MAX_SIDE) * LANDFORM_MAX_SIDE * 3 +
                  sizeof(t_landform_header) <
              std::numeric_limits<uint32_t>::max());
// It must fit the uint8_t layer
static_assert(LANDFORM_MAX_SLOPE_HALF_DEG <= std::numeric_limits<uint8_t>::max());
