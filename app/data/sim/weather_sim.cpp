#include "./weather_sim.hpp"
#include "../link/weather_protocol.hpp"

// Deterministic fake weather so the module can be tested without any real
// data source. Not a climate model. Integer maths only.
constexpr uint32_t SIM_TEMP_POLE_DK = 1600;         // 160 K at the poles
constexpr uint32_t SIM_TEMP_EQUATOR_GAIN_DK = 700;  // up to 230 K at equator
constexpr uint32_t SIM_TEMP_LON_PERIOD = 60;
constexpr uint32_t SIM_TEMP_LON_STEP_DK = 5;
// Datum pressure (0 m): 600 .. 689 Pa
constexpr uint32_t SIM_PRESSURE_BASE_DPA = 6000;
constexpr uint32_t SIM_PRESSURE_LON_PERIOD = 90;
constexpr uint32_t SIM_PRESSURE_LON_STEP_DPA = 10;
constexpr uint32_t SIM_WIND_BASE_CMS = 500;
constexpr uint32_t SIM_WIND_LAT_PERIOD = 10;
constexpr uint32_t SIM_WIND_LAT_STEP_CMS = 100;
constexpr uint32_t SIM_DUST_BASE_X40 = 16;
constexpr uint32_t SIM_DUST_LON_PERIOD = 8;

constexpr uint8_t SIM_WIND_FROM_X256 = 48; // 67.5 deg, ENE

static uint32_t g_sim_time_s = 0;

void weather_sim_set_time(uint32_t time_s) { g_sim_time_s = time_s; }

static t_weather sim_cell(int32_t lat_deg, uint32_t lon_deg) {
  const uint32_t abs_lat = static_cast<uint32_t>(lat_deg < 0 ? -lat_deg : lat_deg);
  const uint32_t lat_max = static_cast<uint32_t>(LAT_MAX_DEG);
  const uint32_t to_pole = abs_lat < lat_max ? lat_max - abs_lat : 0;
  t_weather cell = {};
  // Warmer at the equator, colder toward the poles
  cell.temperature_dk = static_cast<uint16_t>(
      SIM_TEMP_POLE_DK + SIM_TEMP_EQUATOR_GAIN_DK * to_pole / lat_max +
      (lon_deg % SIM_TEMP_LON_PERIOD) * SIM_TEMP_LON_STEP_DK);
  cell.pressure_dpa = static_cast<uint16_t>(
      SIM_PRESSURE_BASE_DPA +
      (lon_deg % SIM_PRESSURE_LON_PERIOD) * SIM_PRESSURE_LON_STEP_DPA);
  cell.wind_cms = static_cast<uint16_t>(
      SIM_WIND_BASE_CMS + (abs_lat % SIM_WIND_LAT_PERIOD) * SIM_WIND_LAT_STEP_CMS);
  cell.dust_x40 =
      static_cast<uint8_t>(SIM_DUST_BASE_X40 + lon_deg % SIM_DUST_LON_PERIOD);
  // Trade-wind like: from the east-north-east everywhere
  cell.wind_from_x256 = SIM_WIND_FROM_X256;
  cell.valid = true;
  return (cell);
}

uint16_t weather_sat_sim(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                         uint16_t resp_cap) {
  uint16_t col = 0;
  uint16_t row = 0;
  if (!weather_decode_request(req, req_len, &col, &row))
    return (0);

  t_weather_chunk chunk = {};
  chunk.col = col;
  chunk.row = row;
  chunk.time_s = g_sim_time_s;
  for (uint32_t y = 0; y < WEATHER_CHUNK_SIDE; y++)
    for (uint32_t x = 0; x < WEATHER_CHUNK_SIDE; x++)
      // Cell in whole degrees: lat in [-90, 94), lon in [0, 360)
      chunk.cells[y * WEATHER_CHUNK_SIDE + x] =
          sim_cell(-LAT_MAX_DEG + static_cast<int32_t>(row * WEATHER_CHUNK_SIDE + y),
                   col * WEATHER_CHUNK_SIDE + x);
  return (weather_encode_chunk(chunk, resp, resp_cap));
}
