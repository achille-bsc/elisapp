#include "./weather_mcd.hpp"
#include "../config.hpp"
#include "../link/weather_protocol.hpp"
#include "../weather/weather_model.hpp"
#include <cstring>

static const uint8_t *g_cells = nullptr;

// Centre of the 1 deg cell `col` east of 0 E / `row` north of 90 S
static uint32_t col_centre_udeg(uint32_t col) {
  return (col * WEATHER_CELL_UDEG + WEATHER_CELL_UDEG / 2);
}

static int32_t row_centre_udeg(uint32_t row) {
  return (static_cast<int32_t>(row * WEATHER_CELL_UDEG + WEATHER_CELL_UDEG / 2) -
          LAT_MAX_UDEG);
}
static t_weather_mcd_header g_header = {};
static uint32_t g_time_s = 0;

bool weather_mcd_set_blob(const void *blob, uint32_t size) {
  g_cells = nullptr;
  g_header = {};
  if (!blob || size < sizeof(t_weather_mcd_header))
    return (blob == nullptr);
  t_weather_mcd_header h = {};
  memcpy(&h, blob, sizeof(h));
  const uint64_t cells = uint64_t{h.hours} * h.rows * h.cols;
  if (h.magic != WEATHER_MCD_MAGIC || h.version != WEATHER_MCD_VERSION ||
      h.hours != WEATHER_MCD_HOURS || h.cols == 0 || h.rows == 0 ||
      uint32_t{h.west_col} + h.cols > LON_MAX_DEG ||
      uint32_t{h.south_row} + h.rows > 2 * LAT_MAX_DEG ||
      size != sizeof(h) + cells * sizeof(t_weather_mcd_cell))
    return (false);
  g_header = h;
  g_cells = static_cast<const uint8_t *>(blob) + sizeof(h);
  return (true);
}

void weather_mcd_set_time(uint32_t time_s) { g_time_s = time_s; }

uint16_t weather_mcd_solar_longitude_ddeg() {
  return (g_cells ? g_header.solar_longitude_ddeg : 0);
}

static t_weather_mcd_cell cell_at(uint32_t hour, uint32_t row, uint32_t col) {
  t_weather_mcd_cell c = {};
  const uint32_t index = (hour * g_header.rows + row) * g_header.cols + col;
  memcpy(&c, g_cells + index * sizeof(c), sizeof(c));
  return (c);
}

static uint16_t mix(uint16_t a, uint16_t b, uint32_t t, uint32_t span) {
  return (static_cast<uint16_t>((uint32_t{a} * (span - t) + uint32_t{b} * t +
                                 span / 2) / span));
}

// Cell (lat_row from 90 S, lon_col from 0 E) at the observation time
static t_weather observe(uint32_t lat_row, uint32_t lon_col) {
  t_weather none = {};
  if (!g_cells || lat_row < g_header.south_row || lon_col < g_header.west_col)
    return (none);
  const uint32_t row = lat_row - g_header.south_row;
  const uint32_t col = lon_col - g_header.west_col;
  if (row >= g_header.rows || col >= g_header.cols)
    return (none);
  // The MCD hours are local solar hours of the cell centre
  const t_coord centre = {col_centre_udeg(lon_col), row_centre_udeg(lat_row)};
  const uint32_t lst = local_solar_time_s(centre, g_time_s);
  const uint32_t hour = lst / MARS_HOUR_S % WEATHER_MCD_HOURS;
  const uint32_t next = (hour + 1) % WEATHER_MCD_HOURS;
  const uint32_t t = lst % MARS_HOUR_S;
  const t_weather_mcd_cell a = cell_at(hour, row, col);
  const t_weather_mcd_cell b = cell_at(next, row, col);
  if (!a.valid || !b.valid)
    return (none);
  t_weather w = {};
  w.temperature_dk = mix(a.temperature_dk, b.temperature_dk, t, MARS_HOUR_S);
  w.pressure_dpa = mix(a.pressure_dpa, b.pressure_dpa, t, MARS_HOUR_S);
  w.wind_cms = mix(a.wind_cms, b.wind_cms, t, MARS_HOUR_S);
  w.dust_x40 = static_cast<uint8_t>(mix(a.dust_x40, b.dust_x40, t, MARS_HOUR_S));
  // Shorter way round between the two hours
  const int32_t turn = static_cast<int8_t>(static_cast<uint8_t>(b.wind_from_x256 - a.wind_from_x256));
  w.wind_from_x256 = static_cast<uint8_t>(
      a.wind_from_x256 + turn * static_cast<int32_t>(t) / static_cast<int32_t>(MARS_HOUR_S));
  w.valid = w.temperature_dk != WEATHER_NO_OBSERVATION_DK;
  return (w);
}

uint16_t weather_sat_mcd(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                         uint16_t resp_cap) {
  uint16_t col = 0;
  uint16_t row = 0;
  if (!weather_decode_request(req, req_len, &col, &row))
    return (0);
  t_weather_chunk chunk = {};
  chunk.col = col;
  chunk.row = row;
  chunk.time_s = g_time_s;
  for (uint32_t y = 0; y < WEATHER_CHUNK_SIDE; y++)
    for (uint32_t x = 0; x < WEATHER_CHUNK_SIDE; x++)
      chunk.cells[y * WEATHER_CHUNK_SIDE + x] =
          observe(row * WEATHER_CHUNK_SIDE + y, col * WEATHER_CHUNK_SIDE + x);
  return (weather_encode_chunk(chunk, resp, resp_cap));
}

static_assert(sizeof(t_weather_mcd_header) == 20);
static_assert(sizeof(t_weather_mcd_cell) == 10);
static_assert(uint64_t{UINT16_MAX} * MARS_HOUR_S * 2 < UINT32_MAX,
              "mix() stays in 32 bits");
