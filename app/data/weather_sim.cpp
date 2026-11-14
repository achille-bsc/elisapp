#include "./weather.hpp"

// SIMULATED satellite: deterministic fake weather so the module can be tested
// without any real data source. Not a climate model. Integer maths only.
constexpr uint32_t SIM_TEMP_POLE_DK = 1600; // 160 K at the poles
constexpr uint32_t SIM_TEMP_EQUATOR_GAIN_DK = 700; // up to 230 K at the equator
constexpr uint32_t SIM_TEMP_LON_PERIOD = 60;
constexpr uint32_t SIM_TEMP_LON_STEP_DK = 5;
constexpr uint32_t SIM_PRESSURE_BASE_DPA = 6000;
constexpr uint32_t SIM_PRESSURE_LON_PERIOD = 90;
constexpr uint32_t SIM_PRESSURE_LON_STEP_DPA = 10;
constexpr uint32_t SIM_WIND_BASE_CMS = 500;
constexpr uint32_t SIM_WIND_LAT_PERIOD = 10;
constexpr uint32_t SIM_WIND_LAT_STEP_CMS = 100;
constexpr uint32_t SIM_DUST_BASE_X40 = 16;
constexpr uint32_t SIM_DUST_LON_PERIOD = 8;

uint16_t weather_sat_sim(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                         uint16_t resp_cap) {
  uint16_t col = 0;
  uint16_t row = 0;
  if (!weather_decode_request(req, req_len, &col, &row))
    return (0);

  t_weather_chunk chunk = {};
  chunk.col = col;
  chunk.row = row;
  chunk.sol_time_s = 0;
  for (uint32_t y = 0; y < WEATHER_CHUNK_SIDE; y++) {
    for (uint32_t x = 0; x < WEATHER_CHUNK_SIDE; x++) {
      // Cell index in degrees: lat in [-90, 90), lon in [0, 360)
      const int32_t lat =
          -LAT_MAX_DEG + static_cast<int32_t>(row * WEATHER_CHUNK_SIDE + y);
      const uint32_t lon = col * WEATHER_CHUNK_SIDE + x;
      const uint32_t abs_lat = static_cast<uint32_t>(lat < 0 ? -lat : lat);
      // Warmer at the equator, colder toward the poles
      const uint32_t temp =
          SIM_TEMP_POLE_DK +
          SIM_TEMP_EQUATOR_GAIN_DK *
              (abs_lat < LAT_MAX_DEG ? LAT_MAX_DEG - abs_lat : 0) / LAT_MAX_DEG +
          (lon % SIM_TEMP_LON_PERIOD) * SIM_TEMP_LON_STEP_DK;
      t_weather &cell = chunk.cells[y * WEATHER_CHUNK_SIDE + x];
      cell.temperature_dk = static_cast<uint16_t>(temp);
      cell.pressure_dpa = static_cast<uint16_t>(
          SIM_PRESSURE_BASE_DPA +
          (lon % SIM_PRESSURE_LON_PERIOD) * SIM_PRESSURE_LON_STEP_DPA);
      cell.wind_cms = static_cast<uint16_t>(
          SIM_WIND_BASE_CMS + (abs_lat % SIM_WIND_LAT_PERIOD) * SIM_WIND_LAT_STEP_CMS);
      cell.dust_x40 = static_cast<uint8_t>(SIM_DUST_BASE_X40 + lon % SIM_DUST_LON_PERIOD);
      cell.valid = true;
    }
  }
  return (weather_encode_chunk(chunk, resp, resp_cap));
}
