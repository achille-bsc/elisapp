#include "./data_background.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../data/data.hpp"

namespace mars::ui {

namespace {

// Structures //
struct s_rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} typedef t_rgb;

// Constants //
constexpr uint32_t BYTE_BITS = 8;
constexpr uint32_t RED_SHIFT = 16;
constexpr uint32_t CHANNEL_MAX = 255;
// RGB565: 5 bits red, 6 green, 5 blue
constexpr uint32_t RGB565_RED_DROP = 3;
constexpr uint32_t RGB565_GREEN_DROP = 2;
constexpr uint32_t RGB565_BLUE_DROP = 3;
constexpr uint32_t RGB565_RED_SHIFT = 11;
constexpr uint32_t RGB565_GREEN_SHIFT = 5;
constexpr uint32_t RGB565_BYTES = 2;

constexpr t_rgb rgb(uint32_t hex) {
  return (t_rgb{static_cast<uint8_t>(hex >> RED_SHIFT & CHANNEL_MAX),
                static_cast<uint8_t>(hex >> BYTE_BITS & CHANNEL_MAX),
                static_cast<uint8_t>(hex & CHANNEL_MAX)});
}

// Terrain ramp: deep basins dark, high ground light (Mars palette)
constexpr t_rgb TERRAIN_LOW = rgb(0x3A1F14);
constexpr t_rgb TERRAIN_MID = rgb(0xA4532C);
constexpr t_rgb TERRAIN_HIGH = rgb(0xE7C8A0);
// Temperature ramp, tenths of kelvin: 170 K blue, 215 K pale, 260 K orange
constexpr t_rgb WEATHER_COLD = rgb(0x2C4F8F);
constexpr t_rgb WEATHER_MILD = rgb(0xD9D4C7);
constexpr t_rgb WEATHER_WARM = rgb(0xE0782A);
constexpr int32_t WEATHER_COLD_DK = 1700;
constexpr int32_t WEATHER_WARM_DK = 2600;
// Risk levels; "coarse" = caution only because the relief is MOLA's 1.85 km
constexpr t_rgb RISK_SAFE = rgb(0x3E8E41);
constexpr t_rgb RISK_COARSE = rgb(0x9DB53A);
constexpr t_rgb RISK_CAUTION = rgb(0xE3A21A);
constexpr t_rgb RISK_DANGER = rgb(0xC62828);
constexpr t_rgb NO_DATA = rgb(0x1E1410);
constexpr t_rgb NO_WEATHER = rgb(0x6B6B6B);
// Reasons that alone make the coarse colour, not the caution one
constexpr uint16_t COARSE_ONLY = HAZARD_COARSE_TERRAIN | HAZARD_NO_WEATHER;
// Shaded relief: brightness = 1 + gain * (rise toward the north-west light)
constexpr double SHADE_GAIN = 6.0;
constexpr double SHADE_MIN = 0.55;
constexpr double SHADE_MAX = 1.4;
constexpr double MM_PER_M = 1000.0;
// Metres per degree of latitude on the Mars sphere (~59.2 km)
constexpr double M_PER_DEG = static_cast<double>(MM_PER_UDEG_LAT_Q16) / Q16_ONE *
                             UDEG_PER_DEG / MM_PER_M;

t_rgb lerp(t_rgb a, t_rgb b, double t) {
  const auto mix = [t](uint8_t from, uint8_t to) {
    return (static_cast<uint8_t>(std::lround(from + (to - from) * t)));
  };
  return (t_rgb{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)});
}

// Three stops ramp, t in [0, 1]
t_rgb ramp(t_rgb low, t_rgb mid, t_rgb high, double t) {
  t = std::clamp(t, 0.0, 1.0);
  return (t < 0.5 ? lerp(low, mid, 2 * t) : lerp(mid, high, 2 * t - 1));
}

t_rgb shade(t_rgb c, double brightness) {
  const auto scale = [brightness](uint8_t v) {
    return (static_cast<uint8_t>(
        std::min<long>(CHANNEL_MAX, std::lround(v * brightness))));
  };
  return (t_rgb{scale(c.r), scale(c.g), scale(c.b)});
}

void put_rgb565(uint8_t *at, t_rgb c) {
  const uint32_t v = (uint32_t{c.r} >> RGB565_RED_DROP) << RGB565_RED_SHIFT |
                     (uint32_t{c.g} >> RGB565_GREEN_DROP) << RGB565_GREEN_SHIFT |
                     uint32_t{c.b} >> RGB565_BLUE_DROP;
  at[0] = static_cast<uint8_t>(v & CHANNEL_MAX); // little endian
  at[1] = static_cast<uint8_t>(v >> BYTE_BITS);
}

t_coord coord_at(const MapProjection &projection, uint32_t x, uint32_t y) {
  const int32_t half = DATA_BACKGROUND_STEP_PX / 2;
  const t_position p = projection.toPosition(
      {static_cast<int32_t>(x) * DATA_BACKGROUND_STEP_PX + half,
       static_cast<int32_t>(y) * DATA_BACKGROUND_STEP_PX + half});
  return (coord_from_e7_east(p.latitudeE7, p.longitudeEastE7));
}

// Elevation of the layer cell (col, row), or `fallback` outside it / in a hole
double cell_elevation(const t_landform_header &layer, int64_t col, int64_t row,
                      double fallback) {
  const int64_t lon = int64_t{layer.west_udeg} +
                      col * layer.step_udeg + layer.step_udeg / 2;
  const int64_t lat = int64_t{layer.north_udeg} -
                      row * layer.step_udeg - layer.step_udeg / 2;
  const t_landform cell = landform_get(
      t_coord{lon_wrap_udeg(lon), static_cast<int32_t>(lat)});
  return (cell.valid ? cell.elevation_m : fallback);
}

// Picture only: elevation interpolated between the four cell centres around
// the point, so zooming in shows a continuous relief instead of 1.85 km
// squares whose edges the shading would turn into a grid. The hazards keep
// the cell values.
bool smooth_elevation(t_coord coord, double *elevation_m) {
  const t_landform here = landform_get(coord);
  if (!here.valid)
    return (false);
  const t_landform_header *const layer = landform_layer(here.layer);
  const double step = layer->step_udeg;
  // Position in cells from the centre of the north-west cell
  const double x = lon_delta_udeg(layer->west_udeg, coord.lon_udeg) / step - 0.5;
  const double y = (double{1} * layer->north_udeg - coord.lat_udeg) / step - 0.5;
  const int64_t col = static_cast<int64_t>(std::floor(x));
  const int64_t row = static_cast<int64_t>(std::floor(y));
  const double tx = x - static_cast<double>(col);
  const double ty = y - static_cast<double>(row);
  const double z = here.elevation_m;
  const double top = (1 - tx) * cell_elevation(*layer, col, row, z) +
                     tx * cell_elevation(*layer, col + 1, row, z);
  const double bottom = (1 - tx) * cell_elevation(*layer, col, row + 1, z) +
                        tx * cell_elevation(*layer, col + 1, row + 1, z);
  *elevation_m = (1 - ty) * top + ty * bottom;
  return (true);
}

t_rgb risk_color(const t_hazard &h) {
  if (h.level == HAZARD_DANGER)
    return (RISK_DANGER);
  if (h.level == HAZARD_CAUTION)
    return ((h.reasons & ~COARSE_ONLY) ? RISK_CAUTION : RISK_COARSE);
  return (RISK_SAFE);
}

} // namespace

data_background::data_background(const IMissionClock &clock) : clock_{clock} {}

const void *data_background::background(MapLayer layer,
                                        const MapProjection &projection) {
  const MapSize size = projection.size();
  if (size.width <= 0 || size.height <= 0)
    return (nullptr);
  const uint32_t w = static_cast<uint32_t>(
      (size.width + DATA_BACKGROUND_STEP_PX - 1) / DATA_BACKGROUND_STEP_PX);
  const uint32_t h = static_cast<uint32_t>(
      (size.height + DATA_BACKGROUND_STEP_PX - 1) / DATA_BACKGROUND_STEP_PX);
  const uint32_t time_s = clock_.nowSeconds();

  // Pass 1: elevations, for the colour ramp and the shading
  constexpr float NO_RELIEF = std::numeric_limits<float>::quiet_NaN();
  elevations_.assign(size_t{w} * h, NO_RELIEF);
  double low = MOLA_MAX_M;
  double high = MOLA_MIN_M;
  for (uint32_t y = 0; y < h; y++)
    for (uint32_t x = 0; x < w; x++) {
      double z = 0;
      if (!smooth_elevation(coord_at(projection, x, y), &z))
        continue;
      elevations_[size_t{y} * w + x] = static_cast<float>(z);
      low = std::min(low, z);
      high = std::max(high, z);
    }
  const double range = high > low ? high - low : 1;
  const double ground_m =
      DATA_BACKGROUND_STEP_PX / projection.pixelsPerDegree() * M_PER_DEG;

  // Pass 2: colours
  t_background_frame &frame = frames_[next_];
  next_ = static_cast<uint8_t>((next_ + 1) % DATA_BACKGROUND_FRAMES);
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
  frame.words.assign((size_t{stride} * h + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
  uint8_t *const bytes = reinterpret_cast<uint8_t *>(frame.words.data());
  for (uint32_t y = 0; y < h; y++)
    for (uint32_t x = 0; x < w; x++) {
      const size_t index = size_t{y} * w + x;
      uint8_t *const at = bytes + size_t{y} * stride + size_t{x} * RGB565_BYTES;
      const double z = elevations_[index];
      if (std::isnan(z)) {
        put_rgb565(at, NO_DATA);
        continue;
      }
      // Rise toward the light: west and north neighbours (holes are flat)
      const double west = x > 0 ? elevations_[index - 1] : z;
      const double north = y > 0 ? elevations_[index - w] : z;
      const double rise = ((std::isnan(west) ? z : west) - z) +
                          ((std::isnan(north) ? z : north) - z);
      const double brightness =
          std::clamp(1.0 + SHADE_GAIN * rise / ground_m, SHADE_MIN, SHADE_MAX);
      t_rgb color = NO_DATA;
      if (layer == MapLayer::Terrain)
        color = ramp(TERRAIN_LOW, TERRAIN_MID, TERRAIN_HIGH, (z - low) / range);
      else if (layer == MapLayer::Weather) {
        const t_weather wx = weather_estimate(coord_at(projection, x, y), time_s);
        color = wx.valid ? ramp(WEATHER_COLD, WEATHER_MILD, WEATHER_WARM,
                                static_cast<double>(wx.temperature_dk - WEATHER_COLD_DK) /
                                    (WEATHER_WARM_DK - WEATHER_COLD_DK))
                         : NO_WEATHER;
      } else
        color = risk_color(hazard_assess(coord_at(projection, x, y), time_s));
      put_rgb565(at, shade(color, brightness));
    }

  if (lv_draw_buf_init(&frame.image, w, h, LV_COLOR_FORMAT_RGB565, stride,
                       frame.words.data(), stride * h) != LV_RESULT_OK)
    return (nullptr);
  // Same address, new pixels: LVGL must not reuse a cached copy
  lv_image_cache_drop(&frame.image);
  return (&frame.image);
}

static_assert(DATA_BACKGROUND_STEP_PX > 0);
static_assert(DATA_BACKGROUND_FRAMES >= 2, "the shown picture stays valid");
static_assert(WEATHER_COLD_DK < WEATHER_WARM_DK);
static_assert(SHADE_MIN < 1.0 && SHADE_MAX > 1.0);
static_assert(rgb(0x123456).r == 0x12 && rgb(0x123456).b == 0x56);

} // namespace mars::ui
