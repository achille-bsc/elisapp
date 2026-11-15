#include "data/core/crc16.hpp"
#include "data/core/endian.hpp"
#include "data/core/fixed_math.hpp"
#include "data/data.hpp"
#include "data/sim/weather_mcd.hpp"
#include "data/sim/weather_sim.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Tests of every public function of app/data. Run: make test
// Usage: data_test [zone_file]
// zone_file: made by tools/fetch_landform.py from the synthetic image of
// tests/make_synthetic_mola.py, checks the python tool end to end.

static int g_failed = 0;
static int g_checks = 0;
static int g_link_calls = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_checks++;                                                                \
    if (!(cond)) {                                                             \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
      g_failed++;                                                              \
    }                                                                          \
  } while (0)

static bool near(int64_t a, int64_t b, int64_t tolerance) {
  return ((a > b ? a - b : b - a) <= tolerance);
}

// Test zone //
// 300x300 cells of 1/32 deg from (75 E, 25 N), around Jezero (77.45 E,
// 18.44 N). Elevation rises eastward, gentle slopes, one cliff band.
constexpr uint32_t ZONE_WEST_UDEG = 75000000;
constexpr int32_t ZONE_NORTH_UDEG = 25000000;
constexpr uint16_t ZONE_SIDE = 300;
constexpr int32_t ZONE_BASE_M = -3000;
constexpr int32_t ZONE_STEP_M = 10; // per column
constexpr uint32_t ZONE_GENTLE_MODULO = 10; // slopes 0 .. 4.5 deg
constexpr uint32_t CLIFF_COL = 150;
constexpr uint32_t CLIFF_WIDTH = 5;
constexpr uint8_t CLIFF_SLOPE_HALF_DEG = 100; // 50 deg
constexpr uint32_t RAMP_COL = 200;
constexpr uint8_t RAMP_SLOPE_HALF_DEG = 35; // 17.5 deg: steep, still walkable
static const t_coord JEZERO = {77450900, 18444500};
// JEZERO in the test zone: (77.4509 - 75) * 32 = col 78,
// (25 - 18.4445) * 32 = row 209
constexpr uint32_t JEZERO_COL = 78;
constexpr uint32_t JEZERO_ROW = 209;

static int16_t zone_elevation(uint32_t col) {
  return (static_cast<int16_t>(ZONE_BASE_M + static_cast<int32_t>(col) * ZONE_STEP_M));
}

static uint8_t zone_slope(uint32_t row, uint32_t col) {
  if (col >= CLIFF_COL && col < CLIFF_COL + CLIFF_WIDTH)
    return (CLIFF_SLOPE_HALF_DEG);
  if (col >= RAMP_COL && col < RAMP_COL + CLIFF_WIDTH)
    return (RAMP_SLOPE_HALF_DEG);
  return (static_cast<uint8_t>(row % ZONE_GENTLE_MODULO));
}

static t_coord zone_cell_centre(uint32_t row, uint32_t col) {
  return (t_coord{ZONE_WEST_UDEG + col * LANDFORM_MOLA_STEP_UDEG + LANDFORM_MOLA_STEP_UDEG / 2,
                  ZONE_NORTH_UDEG - static_cast<int32_t>(row * LANDFORM_MOLA_STEP_UDEG +
                                                         LANDFORM_MOLA_STEP_UDEG / 2)});
}

static const t_landform_header ZONE_HEADER = {
    LANDFORM_MAGIC, ZONE_WEST_UDEG, ZONE_NORTH_UDEG,  LANDFORM_MOLA_STEP_UDEG,
    ZONE_SIDE,      ZONE_SIDE,      LANDFORM_VERSION, 0, 0};

// Blob in uint32_t words: 4 bytes aligned like a flash
static std::vector<uint32_t> make_blob(t_landform_header header,
                                       uint32_t *size,
                                       int16_t flat_m = LANDFORM_NODATA_M) {
  const uint32_t count = static_cast<uint32_t>(header.width) * header.height;
  *size = sizeof(header) + count * LANDFORM_BYTES_PER_CELL;
  std::vector<uint32_t> blob((*size + sizeof(uint32_t) - 1) / sizeof(uint32_t));
  uint8_t *bytes = reinterpret_cast<uint8_t *>(blob.data());
  memcpy(bytes, &header, sizeof(header));
  int16_t *elevation = reinterpret_cast<int16_t *>(bytes + sizeof(header));
  uint8_t *slope = bytes + sizeof(header) + count * sizeof(int16_t);
  for (uint32_t i = 0; i < count; i++) {
    // flat_m: one flat elevation everywhere, else the test zone pattern
    const bool flat = flat_m != LANDFORM_NODATA_M;
    elevation[i] = flat ? flat_m : zone_elevation(i % header.width);
    slope[i] = flat ? 0 : zone_slope(i / header.width, i % header.width);
    slope[count + i] = slope[i]; // MOLA cells are wider than the clearance
  }
  return (blob);
}

static std::vector<uint32_t> g_zone;

static void load_zone() {
  uint32_t size = 0;
  g_zone = make_blob(ZONE_HEADER, &size);
  CHECK(landform_init(g_zone.data(), size));
}

// Links //
static uint16_t counting_link(const uint8_t *req, uint16_t req_len,
                              uint8_t *resp, uint16_t resp_cap) {
  g_link_calls++;
  return (weather_sat_sim(req, req_len, resp, resp_cap));
}

static uint16_t dead_link(const uint8_t *, uint16_t, uint8_t *, uint16_t) {
  return (0);
}

constexpr uint16_t CORRUPT_BYTE = 20;
static uint16_t corrupt_link(const uint8_t *req, uint16_t req_len,
                             uint8_t *resp, uint16_t resp_cap) {
  const uint16_t len = weather_sat_sim(req, req_len, resp, resp_cap);
  if (len)
    resp[CORRUPT_BYTE] ^= BYTE_MASK;
  return (len);
}

// Answers a chunk that was not asked for
static uint16_t wrong_chunk_link(const uint8_t *, uint16_t, uint8_t *resp,
                                 uint16_t resp_cap) {
  uint8_t other[WEATHER_REQUEST_SIZE];
  weather_encode_request(0, 0, other, sizeof(other));
  return (weather_sat_sim(other, sizeof(other), resp, resp_cap));
}

// Same chunk with every cell changed by `edit`
template <void (*EDIT)(t_weather &)>
static uint16_t edited_link(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                            uint16_t resp_cap) {
  const uint16_t len = weather_sat_sim(req, req_len, resp, resp_cap);
  t_weather_chunk chunk = {};
  if (!weather_decode_chunk(resp, len, &chunk))
    return (0);
  for (t_weather &cell : chunk.cells)
    EDIT(cell);
  return (weather_encode_chunk(chunk, resp, resp_cap));
}

static void storm_wind(t_weather &cell) {
  cell.wind_cms = HAZARD_WIND_CMS + 1;
}

static void storm_dust(t_weather &cell) {
  cell.dust_x40 = HAZARD_DUST_X40 + 1;
}

// core/ //
static void test_coord() {
  CHECK(coord_is_valid({0, 0}));
  CHECK(coord_is_valid({LON_MAX_UDEG - 1, LAT_MAX_UDEG}));
  CHECK(coord_is_valid({0, -LAT_MAX_UDEG}));
  CHECK(!coord_is_valid({LON_MAX_UDEG, 0}));
  CHECK(!coord_is_valid({0, -LAT_MAX_UDEG - 1}));

  // E7 rounding, both signs
  CHECK(e7_to_udeg(0) == 0);
  CHECK(e7_to_udeg(4) == 0 && e7_to_udeg(5) == 1 && e7_to_udeg(-5) == -1);
  CHECK(lon_wrap_udeg(-1) == LON_MAX_UDEG - 1);
  CHECK(lon_wrap_udeg(LON_MAX_UDEG) == 0);
  CHECK(lon_wrap_udeg(int64_t{LON_MAX_UDEG} * 3 + 7) == 7);

  // Pathfinding GeoPoint (signed lon) and GPS packet (unsigned east lon)
  const t_coord west = coord_from_e7(-123456789, -1234567890);
  CHECK(west.lat_udeg == -12345679);
  CHECK(west.lon_udeg == LON_MAX_UDEG - 123456789);
  CHECK(coord_lon_e7(west) == -1234567890);
  CHECK(coord_lat_e7(west) == -123456790);
  const t_coord gps = coord_from_e7_east(184446000, 774509000);
  CHECK(gps.lon_udeg == 77450900 && gps.lat_udeg == 18444600);
  CHECK(coord_from_e7_east(0, 3599999999u).lon_udeg == 0); // rounds to 360
  // 180 deg is east, just past it is west
  CHECK(coord_lon_e7({LON_HALF_UDEG, 0}) == static_cast<int32_t>(LON_HALF_UDEG) * E7_PER_UDEG);
  CHECK(coord_lon_e7({LON_HALF_UDEG + 1, 0}) < 0);

  CHECK(lon_delta_udeg(10, 20) == 10 && lon_delta_udeg(20, 10) == -10);
  CHECK(lon_delta_udeg(LON_MAX_UDEG - 5, 5) == 10);
  CHECK(lon_delta_udeg(5, LON_MAX_UDEG - 5) == -10);
  CHECK(lon_delta_udeg(0, LON_HALF_UDEG) == static_cast<int32_t>(LON_HALF_UDEG));
}

static void test_endian_crc() {
  uint8_t buf[sizeof(uint32_t)] = {};
  put_u16(buf, 0xBEEF);
  CHECK(buf[0] == 0xEF && buf[1] == 0xBE && get_u16(buf) == 0xBEEF);
  put_u32(buf, 0xDEADBEEF);
  CHECK(get_u32(buf) == 0xDEADBEEF && buf[3] == 0xDE);
  CHECK(crc16(CRC16_CHECK_INPUT, sizeof(CRC16_CHECK_INPUT)) == 0x29B1);
  CHECK(crc16(CRC16_CHECK_INPUT, 1) != crc16(CRC16_CHECK_INPUT, 2));
}

static void test_fixed_math() {
  CHECK(div_round(1, 2) == 1 && div_round(-1, 2) == -1);
  CHECK(div_round(1, 3) == 0 && div_round(-1, 3) == 0);
  CHECK(div_round(10, 5) == 2 && div_round(-10, 5) == -2);

  // cos_q30 against std::cos on the whole range, every 0.037 deg
  constexpr uint32_t COS_SAMPLE_UDEG = 37000;
  for (uint32_t udeg = 0; udeg < LAT_MAX_UDEG; udeg += COS_SAMPLE_UDEG) {
    const double ref = std::cos(udeg / static_cast<double>(UDEG_PER_DEG) * PI /
                                DEG_PER_HALF_TURN) * static_cast<double>(Q30_ONE);
    CHECK(near(cos_q30(udeg), std::llround(ref), COS_Q30_TOLERANCE));
  }
  CHECK(cos_q30(LAT_MAX_UDEG + 1) == 0); // clamped past 90 deg

  // periodic_cos_q15 against std::cos, error below 0.3 % (48 steps)
  constexpr uint32_t PERIOD = MARS_SOL_S;
  constexpr uint32_t PERIOD_SAMPLE = 997;
  constexpr int64_t PERIODIC_TOLERANCE_Q15 = Q15_ONE / 300;
  for (uint32_t phase = 0; phase < 2 * PERIOD; phase += PERIOD_SAMPLE) {
    const double ref = std::cos(2.0 * PI * (phase % PERIOD) / PERIOD) * Q15_ONE;
    CHECK(near(periodic_cos_q15(phase, PERIOD), std::llround(ref),
               PERIODIC_TOLERANCE_Q15));
  }

  // isqrt_u64 is exact floor(sqrt)
  constexpr uint64_t SQRT_SAMPLES = 5000;
  for (uint64_t n = 0; n < SQRT_SAMPLES; n++) {
    const uint64_t r = isqrt_u64(n);
    CHECK(r * r <= n && (r + 1) * (r + 1) > n);
  }
  const uint64_t big = uint64_t{3000000000u} * 3000000000u + 12345;
  CHECK(isqrt_u64(big) == 3000000000u);
  CHECK(isqrt_u64(UINT64_MAX) == UINT32_MAX);
}

// link/ //
static void test_protocol() {
  uint8_t req[WEATHER_REQUEST_SIZE];
  uint16_t col = 0;
  uint16_t row = 0;
  CHECK(weather_encode_request(21, 14, req, sizeof(req)) == sizeof(req));
  CHECK(weather_decode_request(req, sizeof(req), &col, &row));
  CHECK(col == 21 && row == 14);
  CHECK(weather_encode_request(1, 1, req, sizeof(req) - 1) == 0);
  CHECK(weather_encode_request(1, 1, nullptr, sizeof(req)) == 0);
  CHECK(!weather_decode_request(nullptr, sizeof(req), &col, &row));
  CHECK(!weather_decode_request(req, sizeof(req) - 1, &col, &row));

  uint8_t bad[WEATHER_REQUEST_SIZE];
  memcpy(bad, req, sizeof(req));
  bad[WEATHER_OFF_COL] ^= 1; // CRC no longer matches
  CHECK(!weather_decode_request(bad, sizeof(bad), &col, &row));
  memcpy(bad, req, sizeof(req));
  bad[0] ^= 1; // wrong magic
  CHECK(!weather_decode_request(bad, sizeof(bad), &col, &row));
  memcpy(bad, req, sizeof(req));
  bad[WEATHER_OFF_VERSION] = WEATHER_VERSION + 1;
  CHECK(!weather_decode_request(bad, sizeof(bad), &col, &row));
  // A response is not a request
  memcpy(bad, req, sizeof(req));
  bad[WEATHER_OFF_TYPE] = WEATHER_TYPE_RESPONSE;
  CHECK(!weather_decode_request(bad, sizeof(bad), &col, &row));
  // Chunk id outside the planet
  weather_encode_request(WEATHER_CHUNK_COLS, 0, req, sizeof(req));
  CHECK(!weather_decode_request(req, sizeof(req), &col, &row));
  weather_encode_request(0, WEATHER_CHUNK_ROWS, req, sizeof(req));
  CHECK(!weather_decode_request(req, sizeof(req), &col, &row));
}

static void test_chunk_roundtrip() {
  t_weather_chunk in = {};
  in.col = 9;
  in.row = 13;
  in.time_s = 123456789;
  for (uint32_t i = 0; i < WEATHER_CHUNK_CELLS; i++)
    in.cells[i] = {static_cast<uint16_t>(2000 + i), static_cast<uint16_t>(6000 + i),
                   static_cast<uint16_t>(300 + i), static_cast<uint8_t>(i),
                   static_cast<uint8_t>(4 * i), true, 0};
  in.cells[7] = {UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT8_MAX, UINT8_MAX, true, 0}; // limits

  uint8_t frame[WEATHER_RESPONSE_SIZE];
  CHECK(weather_encode_chunk(in, frame, sizeof(frame) - 1) == 0);
  CHECK(weather_encode_chunk(in, nullptr, sizeof(frame)) == 0);
  CHECK(weather_encode_chunk(in, frame, sizeof(frame)) == sizeof(frame));
  CHECK(weather_frame_is_valid(frame, sizeof(frame)));
  CHECK(!weather_frame_is_valid(frame, sizeof(frame) - 1));
  CHECK(!weather_frame_is_valid(nullptr, sizeof(frame)));
  CHECK(weather_frame_col(frame) == 9 && weather_frame_row(frame) == 13);
  CHECK(weather_frame_time(frame) == 123456789);
  CHECK(weather_frame_cell(frame, 7).temperature_dk == UINT16_MAX);

  t_weather_chunk out = {};
  CHECK(weather_decode_chunk(frame, sizeof(frame), &out));
  CHECK(!weather_decode_chunk(frame, sizeof(frame), nullptr));
  CHECK(out.col == 9 && out.row == 13 && out.time_s == 123456789);
  for (uint32_t i = 0; i < WEATHER_CHUNK_CELLS; i++) {
    CHECK(out.cells[i].temperature_dk == in.cells[i].temperature_dk);
    CHECK(out.cells[i].pressure_dpa == in.cells[i].pressure_dpa);
    CHECK(out.cells[i].wind_cms == in.cells[i].wind_cms);
    CHECK(out.cells[i].dust_x40 == in.cells[i].dust_x40);
    CHECK(out.cells[i].wind_from_x256 == in.cells[i].wind_from_x256);
    CHECK(out.cells[i].valid);
  }

  // A bad frame must not touch the destination
  t_weather_chunk untouched = {};
  untouched.col = 42;
  frame[WEATHER_HEADER_SIZE + 1] ^= 0x10;
  CHECK(!weather_decode_chunk(frame, sizeof(frame), &untouched));
  CHECK(untouched.col == 42);
  // A chunk outside the planet is refused even with a good CRC
  in.col = WEATHER_CHUNK_COLS;
  weather_encode_chunk(in, frame, sizeof(frame));
  CHECK(!weather_frame_is_valid(frame, sizeof(frame)));
}

// weather/ //
static void test_weather_cache() {
  weather_set_link(nullptr);
  CHECK(!weather_get(JEZERO).valid); // no link registered yet

  weather_sim_set_time(4242);
  weather_set_link(counting_link);
  g_link_calls = 0;
  uint32_t time_s = 0;
  const t_weather a = weather_get(JEZERO, &time_s);
  CHECK(a.valid && time_s == 4242);
  CHECK(a.temperature_dk > 1000 && a.temperature_dk < 3200); // 100..320 K
  CHECK(a.pressure_dpa >= 6000 && a.pressure_dpa < 7000);    // datum 600..700
  CHECK(g_link_calls == 1);

  // Same 8x8 deg chunk: served from the cache
  weather_get({JEZERO.lon_udeg + UDEG_PER_DEG,
               JEZERO.lat_udeg + static_cast<int32_t>(UDEG_PER_DEG)});
  CHECK(g_link_calls == 1);
  // Neighbour chunk: one more exchange, then cached too
  weather_get({JEZERO.lon_udeg + WEATHER_CHUNK_UDEG, JEZERO.lat_udeg});
  CHECK(g_link_calls == 2);
  weather_get(JEZERO);
  CHECK(g_link_calls == 2);

  // Fill the cache past its size: oldest chunks leave, nothing breaks
  for (uint32_t i = 0; i < 2u * WEATHER_CACHE_SLOTS; i++)
    CHECK(weather_get({i * WEATHER_CHUNK_UDEG, 0}).valid);
  const int calls = g_link_calls;
  CHECK(weather_get(JEZERO).valid);
  CHECK(g_link_calls == calls + 1); // evicted, asked again

  // Planet edges
  CHECK(weather_get({0, -LAT_MAX_UDEG}).valid);
  CHECK(weather_get({LON_MAX_UDEG - 1, LAT_MAX_UDEG}).valid);
  CHECK(!weather_get({LON_MAX_UDEG, 0}).valid);

  weather_set_link(dead_link);
  CHECK(!weather_get(JEZERO).valid);
  weather_set_link(corrupt_link);
  CHECK(!weather_get(JEZERO).valid);
  weather_set_link(wrong_chunk_link);
  CHECK(!weather_get(JEZERO).valid);
  weather_sim_set_time(0);
  weather_set_link(weather_sat_sim);
  CHECK(weather_get(JEZERO).valid);
}

// Mission time at which `coord` has the local solar time `lst_s`
static uint32_t time_for_lst(t_coord coord, uint32_t lst_s) {
  const uint32_t offset = local_solar_time_s(coord, 0);
  return ((lst_s + MARS_SOL_S - offset) % MARS_SOL_S);
}

static void test_weather_model() {
  CHECK(local_solar_time_s({0, 0}, 0) == 0);
  CHECK(local_solar_time_s({0, 0}, MARS_SOL_S + 5) == 5);
  CHECK(local_solar_time_s({LON_HALF_UDEG, 0}, 0) == MARS_SOL_S / 2);
  CHECK(local_solar_time_s(JEZERO, time_for_lst(JEZERO, 1234)) == 1234);

  // exp(-z / H) against std::exp, interpolation error below 0.2 %
  constexpr int32_t Z_SAMPLE_M = 37;
  constexpr int64_t RATIO_TOLERANCE_DIVISOR = 500;
  CHECK(pressure_ratio_q16(0) == Q16_ONE);
  for (int32_t z = PRESSURE_TABLE_MIN_M; z <= PRESSURE_TABLE_MAX_M; z += Z_SAMPLE_M) {
    const int64_t ref = std::llround(
        std::exp(-static_cast<double>(z) / MARS_SCALE_HEIGHT_M) * Q16_ONE);
    CHECK(near(pressure_ratio_q16(z), ref, ref / RATIO_TOLERANCE_DIVISOR + 1));
  }
  CHECK(pressure_ratio_q16(PRESSURE_TABLE_MIN_M - 1) ==
        pressure_ratio_q16(PRESSURE_TABLE_MIN_M));
  CHECK(pressure_ratio_q16(PRESSURE_TABLE_MAX_M + 1) ==
        pressure_ratio_q16(PRESSURE_TABLE_MAX_M));
  CHECK(pressure_ratio_q16(-1000) > pressure_ratio_q16(1000)); // lower = more air
  CHECK(pressure_at_elevation_dpa(6000, 0) == 6000);
  // Half a scale height down: * sqrt(e) = 1.6487
  CHECK(near(pressure_at_elevation_dpa(6000, -MARS_SCALE_HEIGHT_M / 2), 9892, 20));
  // Below the table (deeper than Hellas): clamped, not extrapolated
  CHECK(pressure_at_elevation_dpa(6000, -2 * MARS_SCALE_HEIGHT_M) ==
        pressure_at_elevation_dpa(6000, PRESSURE_TABLE_MIN_M));
  CHECK(pressure_at_elevation_dpa(UINT16_MAX, PRESSURE_TABLE_MIN_M) == UINT16_MAX);

  // Diurnal delta: 0 at the observation time, +2 at the peak seen from the
  // opposite hour
  const uint32_t peak = PRESSURE_PEAK_HOUR * MARS_HOUR_S;
  CHECK(diurnal_delta_q15(1000, 1000, peak) == 0);
  CHECK(near(diurnal_delta_q15(peak, peak + MARS_SOL_S / 2, peak), 2 * Q15_ONE,
             Q15_ONE / 100));
  CHECK(near(diurnal_delta_q15(peak + MARS_SOL_S / 2, peak, peak), -2 * Q15_ONE,
             Q15_ONE / 100));

  // weather_estimate needs the zone and the satellite
  weather_set_link(nullptr);
  CHECK(!weather_estimate(JEZERO, 0).valid);
  weather_sim_set_time(0);
  weather_set_link(weather_sat_sim);
  CHECK(!weather_estimate({0, 0}, 0).valid); // outside the zone

  // At the observation time: only the altitude correction
  const t_weather sat = weather_get(JEZERO);
  const t_weather now = weather_estimate(JEZERO, 0);
  CHECK(now.valid);
  CHECK(now.pressure_dpa ==
        pressure_at_elevation_dpa(sat.pressure_dpa,
                                  zone_elevation(JEZERO_COL)));
  CHECK(now.pressure_dpa > sat.pressure_dpa); // Jezero is below the datum
  CHECK(now.temperature_dk == sat.temperature_dk);
  CHECK(now.wind_cms == sat.wind_cms && now.dust_x40 == sat.dust_x40);

  // Lower ground, same satellite cell: more pressure
  // Columns 1 and 30 share the 75..76 deg E satellite cell
  const t_coord low = zone_cell_centre(JEZERO_ROW, 1);
  const t_coord high = zone_cell_centre(JEZERO_ROW, 30);
  CHECK(weather_get(low).pressure_dpa == weather_get(high).pressure_dpa);
  CHECK(weather_estimate(low, 0).pressure_dpa > weather_estimate(high, 0).pressure_dpa);

  // Diurnal cycle: hot at the peak hour, cold half a sol later, same day
  const uint32_t hot = time_for_lst(JEZERO, TEMPERATURE_PEAK_HOUR * MARS_HOUR_S);
  const uint32_t cold = time_for_lst(
      JEZERO, (TEMPERATURE_PEAK_HOUR * MARS_HOUR_S + MARS_SOL_S / 2) % MARS_SOL_S);
  const t_weather w_hot = weather_estimate(JEZERO, hot);
  const t_weather w_cold = weather_estimate(JEZERO, cold);
  CHECK(near(w_hot.temperature_dk - w_cold.temperature_dk,
             2 * TEMPERATURE_DIURNAL_AMPLITUDE_DK, 2 * TEMPERATURE_DIURNAL_AMPLITUDE_DK / 100));
  // Long trip: the same hour 10 sols later gives the same estimate
  constexpr uint32_t LONG_TRIP_SOLS = 10;
  CHECK(weather_estimate(JEZERO, hot + LONG_TRIP_SOLS * MARS_SOL_S).temperature_dk ==
        w_hot.temperature_dk);
  // Thermal tide: +-5 % around the altitude corrected pressure
  const uint32_t tide_high = time_for_lst(JEZERO, PRESSURE_PEAK_HOUR * MARS_HOUR_S);
  const uint32_t tide_low = time_for_lst(
      JEZERO, (PRESSURE_PEAK_HOUR * MARS_HOUR_S + MARS_SOL_S / 2) % MARS_SOL_S);
  const int64_t swing = weather_estimate(JEZERO, tide_high).pressure_dpa -
                        weather_estimate(JEZERO, tide_low).pressure_dpa;
  CHECK(near(swing, now.pressure_dpa * 2 * PRESSURE_DIURNAL_AMPLITUDE_PERMILLE / PERMILLE,
             now.pressure_dpa / 100));
}

// radiation/ //
static void test_radiation() {
  // Reference point of the fit: ~0.218 mGy/sol (Hassler 2014: 0.21 +- 0.04)
  const uint16_t ref = radiation_dose_ugy_day(RAD_PRESSURE_REF_DPA, SOLAR_MODULATION_MV);
  CHECK(ref == 218);
  // +100 Pa of air: -13 uGy/sol; +100 MV of solar modulation: -11 uGy/sol
  constexpr uint16_t HUNDRED_PA_DPA = 1000;
  constexpr int32_t HUNDRED_MV = 100;
  CHECK(near(radiation_dose_ugy_day(RAD_PRESSURE_REF_DPA + HUNDRED_PA_DPA, SOLAR_MODULATION_MV),
             ref - 13, 1));
  CHECK(near(radiation_dose_ugy_day(RAD_PRESSURE_REF_DPA, SOLAR_MODULATION_MV + HUNDRED_MV),
             ref - 11, 1));
  // Limits: never negative, never wraps
  CHECK(radiation_dose_ugy_day(UINT16_MAX, SOLAR_MODULATION_MV) == 0);
  CHECK(radiation_dose_ugy_day(0, INT32_MIN) == UINT16_MAX);

  CHECK(!radiation_estimate({0, 0}, 0).valid); // outside the zone

  // No satellite: standard datum pressure corrected by the elevation
  weather_set_link(nullptr);
  const t_radiation offline = radiation_estimate(JEZERO, 0);
  CHECK(offline.valid);
  CHECK(offline.dose_ugy_day ==
        radiation_dose_ugy_day(pressure_at_elevation_dpa(DATUM_PRESSURE_DEFAULT_DPA,
                                                         zone_elevation(JEZERO_COL)),
                               SOLAR_MODULATION_MV));
  // With the satellite: the local pressure of the weather model
  weather_set_link(weather_sat_sim);
  const t_radiation online = radiation_estimate(JEZERO, 0);
  CHECK(online.valid);
  CHECK(online.dose_ugy_day ==
        radiation_dose_ugy_day(weather_estimate(JEZERO, 0).pressure_dpa,
                               SOLAR_MODULATION_MV));
  CHECK(online.dose_eq_usv_day ==
        (online.dose_ugy_day * RAD_QUALITY_FACTOR_PERCENT + PERCENT / 2) / PERCENT);
  CHECK(!online.critical); // GCR alone stays below the NASA based critical rate
  // Even with no air at all the GCR fit stays below the critical rate
  CHECK(!radiation_at_pressure(0).critical);
  CHECK(!radiation_from(t_landform{}, weather_estimate(JEZERO, 0)).valid);

  // Topography: a deeper point has more air above it, so less dose
  const t_coord low = zone_cell_centre(JEZERO_ROW, 1);
  const t_coord high = zone_cell_centre(JEZERO_ROW, ZONE_SIDE - 1);
  CHECK(radiation_estimate(low, 0).dose_ugy_day < radiation_estimate(high, 0).dose_ugy_day);
}

// landform/ //
static void test_landform() {
  // Nothing loaded: a fresh init with a bad blob unloads the zone
  CHECK(!landform_init(nullptr, 0));
  CHECK(landform_layer_count() == 0 && landform_layer(0) == nullptr);
  CHECK(!landform_get(JEZERO).valid);

  uint32_t size = 0;
  std::vector<uint32_t> blob = make_blob(ZONE_HEADER, &size);
  CHECK(landform_init(blob.data(), size));
  CHECK(landform_layer_count() == 1 && landform_layer(0)->width == ZONE_SIDE);
  const t_landform l = landform_get(JEZERO);
  CHECK(l.valid);
  CHECK(l.elevation_m == zone_elevation(JEZERO_COL));
  CHECK(l.slope_half_deg == zone_slope(JEZERO_ROW, JEZERO_COL));
  CHECK(l.layer == 0);

  // Zone edges: inclusive on the north/west, exclusive on the south/east
  const uint32_t east = ZONE_WEST_UDEG + ZONE_SIDE * LANDFORM_MOLA_STEP_UDEG;
  const int32_t south =
      ZONE_NORTH_UDEG - static_cast<int32_t>(ZONE_SIDE * LANDFORM_MOLA_STEP_UDEG);
  CHECK(landform_get({ZONE_WEST_UDEG, ZONE_NORTH_UDEG}).valid);
  CHECK(landform_get({ZONE_WEST_UDEG, ZONE_NORTH_UDEG}).elevation_m ==
        zone_elevation(0));
  CHECK(landform_get({east - 1, south + 1}).valid);
  CHECK(!landform_get({ZONE_WEST_UDEG - 1, JEZERO.lat_udeg}).valid);
  CHECK(!landform_get({JEZERO.lon_udeg, ZONE_NORTH_UDEG + 1}).valid);
  CHECK(!landform_get({east, JEZERO.lat_udeg}).valid);
  CHECK(!landform_get({JEZERO.lon_udeg, south}).valid);
  CHECK(!landform_get({LON_MAX_UDEG, JEZERO.lat_udeg}).valid);
  // Every cell centre reads its own cell
  for (uint32_t row = 0; row < ZONE_SIDE; row += 7)
    for (uint32_t col = 0; col < ZONE_SIDE; col += 7) {
      const t_landform c = landform_get(zone_cell_centre(row, col));
      CHECK(c.valid && c.elevation_m == zone_elevation(col) &&
            c.slope_half_deg == zone_slope(row, col));
    }

  // Refused blobs leave nothing loaded
  CHECK(!landform_init(blob.data(), size - 1)); // truncated
  CHECK(!landform_get(JEZERO).valid);
  CHECK(!landform_init(blob.data(), size + 1)); // padded
  CHECK(!landform_init(blob.data(), sizeof(t_landform_header) - 1));
  CHECK(!landform_init(reinterpret_cast<uint8_t *>(blob.data()) + 2, size - 2));
  t_landform_header bad = ZONE_HEADER;
  bad.magic ^= 1;
  std::vector<uint32_t> other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad = ZONE_HEADER;
  bad.version = LANDFORM_VERSION + 1;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad = ZONE_HEADER;
  bad.width = 0;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad = ZONE_HEADER;
  bad.height = LANDFORM_MAX_SIDE + 1;
  bad.width = 1;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad = ZONE_HEADER;
  bad.west_udeg = LON_MAX_UDEG;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad = ZONE_HEADER;
  bad.north_udeg = LAT_MAX_UDEG + 1;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad = ZONE_HEADER;
  bad.step_udeg = 0;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  bad.step_udeg = LANDFORM_MAX_STEP_UDEG + 1;
  other = make_blob(bad, &size);
  CHECK(!landform_init(other.data(), size));
  CHECK(landform_layer_count() == 0);
}

// gathering //
static void test_gathering() {
  load_zone();
  weather_set_link(weather_sat_sim);
  const t_data_point land = data_gathering(JEZERO, 0, DATA_LANDFORM);
  CHECK(land.landform.valid && !land.weather.valid && !land.radiation.valid);
  const t_data_point wx = data_gathering(JEZERO, 0, DATA_WEATHER);
  CHECK(wx.weather.valid && !wx.landform.valid && !wx.radiation.valid);
  const t_data_point rad = data_gathering(JEZERO, 0, DATA_RADIATION);
  CHECK(rad.radiation.valid && !rad.landform.valid && !rad.weather.valid);
  const t_data_point all = data_gathering(JEZERO, 0, DATA_ALL);
  CHECK(all.landform.valid && all.weather.valid && all.radiation.valid);
  CHECK(all.weather.pressure_dpa == weather_estimate(JEZERO, 0).pressure_dpa);

  // Hazards
  CHECK(!data_is_hazard(JEZERO, 0));                      // flat, calm
  CHECK(data_is_hazard({0, 0}, 0));                       // unknown ground
  CHECK(data_is_hazard(zone_cell_centre(JEZERO_ROW, CLIFF_COL), 0)); // cliff
  CHECK(!data_is_hazard(zone_cell_centre(JEZERO_ROW, CLIFF_COL - 1), 0));
  weather_set_link(edited_link<storm_wind>);
  CHECK(data_is_hazard(JEZERO, 0));
  weather_set_link(edited_link<storm_dust>);
  CHECK(data_is_hazard(JEZERO, 0));
  weather_set_link(nullptr);
  CHECK(!data_is_hazard(JEZERO, 0)); // no weather is not a hazard by itself
  weather_set_link(weather_sat_sim);
}

// grid/ //
// Double precision copy of marspath LocalProjection + CoarseGrid::configure
// (app/core/pathfinding) with the GridParams defaults: our integer grid must
// give the same cells.
struct s_ref_grid {
  double lat0_e7, lon0_e7, m_per_unit_x, m_per_unit_y;
  double ox, oy, ux, uy, cell_m;
  uint32_t width, height;
} typedef t_ref_grid;

constexpr double REF_E7_TO_RAD = 1e-7 * PI / DEG_PER_HALF_TURN;
constexpr double MM_PER_M = 1000.0;

static t_ref_grid ref_configure(t_coord start, t_coord goal) {
  t_ref_grid g = {};
  const double radius_m = MARS_RADIUS_MM / MM_PER_M;
  // marspath midpoint(): int64 sum / 2, truncated toward zero
  g.lat0_e7 = std::trunc((static_cast<double>(coord_lat_e7(start)) + coord_lat_e7(goal)) / 2);
  g.lon0_e7 = std::trunc((static_cast<double>(coord_lon_e7(start)) + coord_lon_e7(goal)) / 2);
  g.m_per_unit_y = REF_E7_TO_RAD * radius_m;
  g.m_per_unit_x = g.m_per_unit_y * std::cos(g.lat0_e7 * REF_E7_TO_RAD);
  const double ax = (coord_lon_e7(start) - g.lon0_e7) * g.m_per_unit_x;
  const double ay = (coord_lat_e7(start) - g.lat0_e7) * g.m_per_unit_y;
  const double bx = (coord_lon_e7(goal) - g.lon0_e7) * g.m_per_unit_x;
  const double by = (coord_lat_e7(goal) - g.lat0_e7) * g.m_per_unit_y;
  const double length = std::hypot(bx - ax, by - ay);
  const double min_cell = GRID_MIN_CELL_MM / MM_PER_M;
  const double corridor_min = GRID_CORRIDOR_MIN_MM / MM_PER_M;
  const double extension =
      std::max(corridor_min, length * GRID_EXTENSION_PERMILLE / GRID_PERMILLE);
  const double half_width =
      std::max(corridor_min, length * GRID_CORRIDOR_PERMILLE / GRID_PERMILLE);
  g.ux = (bx - ax) / length;
  g.uy = (by - ay) / length;
  g.cell_m = std::max(min_cell, (length + 2.0 * extension) / GRID_MAX_SIDE);
  g.width = static_cast<uint32_t>(std::ceil((length + 2.0 * extension) / g.cell_m)) + 1U;
  g.height = static_cast<uint32_t>(std::ceil(2.0 * half_width / g.cell_m)) + 1U;
  const double across = static_cast<double>((g.height - 1U) / 2U) * g.cell_m;
  g.ox = ax - g.ux * extension + g.uy * across; // - v * across, v = (-uy, ux)
  g.oy = ay - g.uy * extension - g.ux * across;
  return (g);
}

// Centre of a reference cell, in micro-degrees (lat, signed lon)
static void ref_centre(const t_ref_grid &g, uint32_t index, double *lat_udeg,
                       double *lon_udeg) {
  const double i = index % g.width * g.cell_m;
  const double j = static_cast<double>(index / g.width) * g.cell_m;
  const double x = g.ox + g.ux * i - g.uy * j;
  const double y = g.oy + g.uy * i + g.ux * j;
  *lat_udeg = (g.lat0_e7 + y / g.m_per_unit_y) / E7_PER_UDEG;
  *lon_udeg = (g.lon0_e7 + x / g.m_per_unit_x) / E7_PER_UDEG;
}

static void check_against_reference(t_coord start, t_coord goal) {
  t_grid grid = {};
  CHECK(grid_init(&grid, start, goal));
  const t_ref_grid ref = ref_configure(start, goal);
  CHECK(grid.width == ref.width && grid.height == ref.height);
  CHECK(near(grid.cell_mm, std::llround(ref.cell_m * MM_PER_M), 1));
  // Cell centres within 2 micro-degrees (~12 cm)
  constexpr double CENTRE_TOLERANCE_UDEG = 2.0;
  const uint32_t count = grid_cell_count(&grid);
  const uint32_t samples[] = {0, grid.width - 1u, count / 2, count - grid.width, count - 1,
                              grid.start_index, grid.goal_index};
  for (uint32_t index : samples) {
    double lat = 0;
    double lon = 0;
    ref_centre(ref, index, &lat, &lon);
    const t_coord c = grid_cell_coord(&grid, index);
    CHECK(std::fabs(c.lat_udeg - lat) <= CENTRE_TOLERANCE_UDEG);
    CHECK(std::fabs(lon_delta_udeg(lon_wrap_udeg(std::llround(lon)), c.lon_udeg)) <=
          CENTRE_TOLERANCE_UDEG + 1);
  }
}

static void test_grid() {
  t_grid grid = {};
  CHECK(!grid_init(nullptr, JEZERO, JEZERO));
  CHECK(!grid_init(&grid, JEZERO, JEZERO));                    // < 1 m
  CHECK(!grid_init(&grid, {LON_MAX_UDEG, 0}, JEZERO));         // invalid
  CHECK(!grid_init(&grid, {0, LAT_MAX_UDEG}, {1000000, LAT_MAX_UDEG})); // pole
  CHECK(!grid_init(&grid, {0, 0}, {LON_HALF_UDEG - 1, 0}));    // > 1500 km

  // Long trip (~300 km): cell = 1.2 L / 512
  const t_coord goal = {JEZERO.lon_udeg + 3500000, JEZERO.lat_udeg + 3000000};
  CHECK(grid_init(&grid, JEZERO, goal));
  CHECK(near(grid.cell_mm, int64_t{grid.length_mm} * 12 / 10 / GRID_MAX_SIDE, 2));
  CHECK(grid.width >= GRID_MAX_SIDE && grid.width <= GRID_MAX_SIDE + 2);
  check_against_reference(JEZERO, goal);
  // Short trip (5 km): floors of 3 km extension and 100 m cells
  const t_coord near_goal = {JEZERO.lon_udeg + 60000, JEZERO.lat_udeg + 60000};
  CHECK(grid_init(&grid, JEZERO, near_goal));
  CHECK(grid.cell_mm == GRID_MIN_CELL_MM);
  check_against_reference(JEZERO, near_goal);
  // Very short trip (just over 1 m)
  CHECK(grid_init(&grid, JEZERO, {JEZERO.lon_udeg, JEZERO.lat_udeg + 20}));
  check_against_reference(JEZERO, {JEZERO.lon_udeg, JEZERO.lat_udeg + 20});
  // Westward and southward trip
  check_against_reference(goal, JEZERO);

  // Start / goal cells hold the start / goal
  CHECK(grid_init(&grid, JEZERO, goal));
  CHECK(grid.start_index == grid_index_of(&grid, JEZERO));
  CHECK(grid.goal_index == grid_index_of(&grid, goal));
  CHECK(grid.start_index != grid.goal_index);
  // Every cell centre maps back to its own cell
  const uint32_t count = grid_cell_count(&grid);
  uint32_t mismatches = 0;
  for (uint32_t i = 0; i < count; i++)
    mismatches += grid_index_of(&grid, grid_cell_coord(&grid, i)) != i;
  CHECK(mismatches == 0);
  // Points far away are clamped to the border, no overflow
  CHECK(grid_index_of(&grid, {JEZERO.lon_udeg + LON_HALF_UDEG, -JEZERO.lat_udeg}) < count);

  // Neighbours: 3 at a corner, 8 inside, same index maths as marspath
  uint32_t out = 0;
  uint32_t found = 0;
  for (uint32_t k = 0; k < GRID_NEIGHBORS; k++)
    found += grid_neighbor(&grid, 0, k, &out);
  CHECK(found == 3);
  const uint32_t inside = uint32_t{grid.width} * 2 + 2;
  for (uint32_t k = 0; k < GRID_NEIGHBORS; k++) {
    CHECK(grid_neighbor(&grid, inside, k, &out));
    CHECK(out == static_cast<uint32_t>(static_cast<int32_t>(inside) + GRID_STEPS[k].di +
                                       GRID_STEPS[k].dj * grid.width));
  }
  CHECK(!grid_neighbor(&grid, inside, GRID_NEIGHBORS, &out));
  CHECK(!grid_neighbor(&grid, count, 0, &out));
  // Distances
  CHECK(grid_distance_mm(&grid, inside, inside + 1) == grid.cell_mm);
  CHECK(grid_distance_mm(&grid, inside, inside + grid.width) == grid.cell_mm);
  CHECK(near(grid_distance_mm(&grid, inside, inside + grid.width + 1),
             std::llround(grid.cell_mm * std::sqrt(2.0)), 1));
  CHECK(grid_distance_mm(&grid, inside, inside) == 0);

  // Data through the grid
  const t_data_point start_data = grid_cell_data(&grid, grid.start_index, 0, DATA_ALL);
  CHECK(start_data.landform.valid && start_data.weather.valid && start_data.radiation.valid);
  CHECK(grid_cell_is_hazard(&grid, 0, 0) ==
        data_is_hazard(grid_cell_coord(&grid, 0), 0));

  // Across the 0 / 360 deg meridian
  const t_coord west = {LON_MAX_UDEG - 100000, 0};
  const t_coord east = {100000, 0};
  CHECK(grid_init(&grid, west, east));
  CHECK(near(grid.length_mm, 11831000, 1000)); // 0.2 deg ~ 11.8 km
  CHECK(coord_is_valid(grid_cell_coord(&grid, grid.start_index)));
  CHECK(grid_index_of(&grid, grid_cell_coord(&grid, grid.goal_index)) == grid.goal_index);
}


// landform layers: CTX over MOLA //
// Fine layer: 400 x 300 cells of 337 udeg (~20 m) from (77.4 E, 18.5 N), flat,
// with a NODATA hole. JEZERO is in it, outside the hole.
constexpr uint32_t FINE_STEP_UDEG = 337;
constexpr uint32_t FINE_WEST_UDEG = 77400000;
constexpr int32_t FINE_NORTH_UDEG = 18500000;
constexpr uint16_t FINE_WIDTH = 400;
constexpr uint16_t FINE_HEIGHT = 300;
constexpr int16_t FINE_ELEVATION_M = -2500;
constexpr uint32_t FINE_HOLE_FIRST = 100;
constexpr uint32_t FINE_HOLE_END = 150;
static const t_landform_header FINE_HEADER = {
    LANDFORM_MAGIC, FINE_WEST_UDEG, FINE_NORTH_UDEG,  FINE_STEP_UDEG,
    FINE_WIDTH,     FINE_HEIGHT,    LANDFORM_VERSION, 0, 0};

static std::vector<uint32_t> g_fine;

static t_coord fine_cell_centre(uint32_t row, uint32_t col) {
  return (t_coord{FINE_WEST_UDEG + col * FINE_STEP_UDEG + FINE_STEP_UDEG / 2,
                  FINE_NORTH_UDEG -
                      static_cast<int32_t>(row * FINE_STEP_UDEG + FINE_STEP_UDEG / 2)});
}

static void load_fine_layer() {
  uint32_t size = 0;
  g_fine = make_blob(FINE_HEADER, &size, FINE_ELEVATION_M);
  int16_t *elevation = reinterpret_cast<int16_t *>(
      reinterpret_cast<uint8_t *>(g_fine.data()) + sizeof(t_landform_header));
  for (uint32_t r = FINE_HOLE_FIRST; r < FINE_HOLE_END; r++)
    for (uint32_t c = FINE_HOLE_FIRST; c < FINE_HOLE_END; c++)
      elevation[r * FINE_WIDTH + c] = LANDFORM_NODATA_M;
  CHECK(landform_add_layer(g_fine.data(), size));
}

static void test_landform_layers() {
  load_zone();       // MOLA first...
  load_fine_layer(); // ...the finer layer still comes first
  CHECK(landform_layer_count() == 2);
  CHECK(landform_layer(0)->step_udeg == FINE_STEP_UDEG);
  CHECK(landform_layer(1)->step_udeg == LANDFORM_MOLA_STEP_UDEG);
  const t_landform fine = landform_get(JEZERO);
  CHECK(fine.valid && fine.layer == 0 && fine.elevation_m == FINE_ELEVATION_M);
  // In the hole and outside the fine layer: MOLA fills in
  const t_landform hole =
      landform_get(fine_cell_centre(FINE_HOLE_FIRST + 1, FINE_HOLE_FIRST + 1));
  CHECK(hole.valid && hole.layer == 1);
  const t_landform edge = landform_get(fine_cell_centre(FINE_HOLE_FIRST - 1, FINE_HOLE_FIRST));
  CHECK(edge.valid && edge.layer == 0);
  const t_coord far = zone_cell_centre(JEZERO_ROW, 10);
  CHECK(landform_get(far).layer == 1 && landform_get(far).elevation_m == zone_elevation(10));
  // CTX ground is fine terrain, MOLA ground is coarse
  CHECK(!(hazard_assess(JEZERO, 0).reasons & HAZARD_COARSE_TERRAIN));
  CHECK(hazard_assess(far, 0).reasons & HAZARD_COARSE_TERRAIN);
  // At most LANDFORM_MAX_LAYERS layers
  std::vector<uint32_t> extra[LANDFORM_MAX_LAYERS];
  uint32_t size = 0;
  uint32_t added = 0;
  for (std::vector<uint32_t> &blob : extra) {
    blob = make_blob(ZONE_HEADER, &size);
    added += landform_add_layer(blob.data(), size);
  }
  CHECK(added == LANDFORM_MAX_LAYERS - 2u);
  CHECK(landform_layer_count() == LANDFORM_MAX_LAYERS);
  CHECK(landform_layer(LANDFORM_MAX_LAYERS) == nullptr);
  landform_clear();
  CHECK(landform_layer_count() == 0 && !landform_get(JEZERO).valid);
  load_zone();
}

// weather cache allocation //
static t_coord chunk_centre(uint32_t col) {
  return (t_coord{col * WEATHER_CHUNK_UDEG + WEATHER_CHUNK_UDEG / 2, 0});
}

static void test_weather_allocation() {
  weather_sim_set_time(0);
  // Caller owned cache of 3 slots, at an odd address: no alignment needed
  constexpr uint32_t SLOTS = 3;
  std::vector<uint8_t> storage(weather_cache_bytes(SLOTS) + 1);
  CHECK(weather_set_cache(storage.data() + 1, weather_cache_bytes(SLOTS)));
  CHECK(weather_cache_slots() == SLOTS);
  weather_set_link(counting_link);
  g_link_calls = 0;
  weather_get(chunk_centre(0));
  weather_get(chunk_centre(1));
  weather_get(chunk_centre(2));
  CHECK(g_link_calls == 3);
  weather_get(chunk_centre(0)); // hit: chunk 0 is now the most recent
  CHECK(g_link_calls == 3);
  weather_get(chunk_centre(3)); // full: evicts chunk 1, the least recent
  CHECK(g_link_calls == 4);
  weather_get(chunk_centre(0)); // survived (round robin would have dropped it)
  CHECK(g_link_calls == 4);
  weather_get(chunk_centre(1)); // was evicted
  CHECK(g_link_calls == 5);

  CHECK(weather_fetch_chunk(0, 0));
  CHECK(!weather_fetch_chunk(WEATHER_CHUNK_COLS, 0));
  uint16_t col = 0;
  uint16_t row = 0;
  weather_chunk_of(JEZERO, &col, &row);
  CHECK(col == 9 && row == 13); // 72..80 deg E, 14..22 deg N

  // Back to the built-in cache: no storage, or too small for one slot
  CHECK(!weather_set_cache(nullptr, 0));
  CHECK(weather_cache_slots() == WEATHER_CACHE_SLOTS);
  CHECK(!weather_set_cache(storage.data(), WEATHER_SLOT_BYTES - 1));
  CHECK(weather_cache_slots() == WEATHER_CACHE_SLOTS);
  weather_set_link(weather_sat_sim);
}

static void test_grid_prefetch() {
  load_zone();
  t_grid grid = {};
  const t_coord goal = {JEZERO.lon_udeg + 3500000, JEZERO.lat_udeg + 3000000};
  CHECK(grid_init(&grid, JEZERO, goal));
  const uint32_t chunks = grid_weather_chunk_count(&grid);
  // 77..82 deg E, 17..23 deg N: 2 x 2 chunks
  CHECK(chunks == 4);

  // Too small (1 slot): nothing is fetched, the needed count comes back
  std::vector<uint8_t> small(weather_cache_bytes(1));
  weather_set_cache(small.data(), static_cast<uint32_t>(small.size()));
  weather_set_link(counting_link);
  g_link_calls = 0;
  CHECK(grid_weather_prefetch(&grid) == chunks);
  CHECK(g_link_calls == 0);

  // Right size: each chunk once, then the whole grid without the radio
  std::vector<uint8_t> cache(weather_cache_bytes(chunks));
  CHECK(weather_set_cache(cache.data(), static_cast<uint32_t>(cache.size())));
  weather_set_link(counting_link);
  g_link_calls = 0;
  CHECK(grid_weather_prefetch(&grid) == 0);
  CHECK(g_link_calls == static_cast<int>(chunks));
  std::vector<uint8_t> levels(grid_cell_count(&grid));
  CHECK(hazard_map(&grid, 0, levels.data(), grid_cell_count(&grid)) ==
        grid_cell_count(&grid));
  CHECK(g_link_calls == static_cast<int>(chunks)); // the A* never waited

  weather_set_link(dead_link);
  CHECK(grid_weather_prefetch(&grid) == chunks);
  weather_set_cache(nullptr, 0);
  weather_set_link(weather_sat_sim);
}

// hazard //
static void test_hazard() {
  load_zone();
  weather_set_link(weather_sat_sim);
  hazard_set_spe_alert(false);
  CHECK(hazard_level_of(0) == HAZARD_SAFE);
  CHECK(hazard_level_of(HAZARD_NO_WEATHER) == HAZARD_SAFE);
  CHECK(hazard_level_of(HAZARD_STEEP) == HAZARD_CAUTION);
  CHECK(hazard_level_of(HAZARD_STEEP | HAZARD_TOO_STEEP) == HAZARD_DANGER);
  CHECK(hazard_level_of(HAZARD_EVA_TOO_LONG | HAZARD_BEYOND_ZONE) == HAZARD_CAUTION);
  CHECK(hazard_level_of(HAZARD_DOSE_BUDGET) == HAZARD_DANGER);

  // Jezero on MOLA only: walkable, with a coarse terrain caution
  const t_hazard j = hazard_assess(JEZERO, 0);
  CHECK(j.level == HAZARD_CAUTION && j.reasons == HAZARD_COARSE_TERRAIN);
  CHECK(j.dose_eq_usv_day == radiation_estimate(JEZERO, 0).dose_eq_usv_day);
  CHECK(!data_is_hazard(JEZERO, 0));

  // Terrain
  const t_hazard none = hazard_assess({0, 0}, 0);
  CHECK((none.reasons & HAZARD_NO_TERRAIN) && none.level == HAZARD_DANGER);
  const t_hazard ramp = hazard_assess(zone_cell_centre(JEZERO_ROW, RAMP_COL), 0);
  CHECK((ramp.reasons & HAZARD_STEEP) && ramp.level == HAZARD_CAUTION);
  const t_hazard cliff = hazard_assess(zone_cell_centre(JEZERO_ROW, CLIFF_COL), 0);
  CHECK((cliff.reasons & HAZARD_TOO_STEEP) && cliff.level == HAZARD_DANGER);
  CHECK(data_is_hazard(zone_cell_centre(JEZERO_ROW, CLIFF_COL), 0));

  // Weather
  weather_set_link(nullptr);
  const t_hazard blind = hazard_assess(JEZERO, 0);
  CHECK(blind.reasons == (HAZARD_COARSE_TERRAIN | HAZARD_NO_WEATHER));
  CHECK(blind.level == HAZARD_CAUTION);
  weather_set_link(edited_link<storm_wind>);
  CHECK(hazard_assess(JEZERO, 0).reasons & HAZARD_WIND);
  CHECK(hazard_assess(JEZERO, 0).level == HAZARD_DANGER);
  weather_set_link(edited_link<storm_dust>);
  CHECK(hazard_assess(JEZERO, 0).reasons & HAZARD_DUST_STORM);
  weather_set_link(weather_sat_sim);

  // Solar particle event alert: seek shelter everywhere
  hazard_set_spe_alert(true);
  CHECK(hazard_spe_alert());
  CHECK((hazard_assess(JEZERO, 0).reasons & HAZARD_SPE_ALERT) && data_is_hazard(JEZERO, 0));
  hazard_set_spe_alert(false);
  CHECK(!data_is_hazard(JEZERO, 0));

  // Radiation classes (GCR alone never reaches them on Mars, see test_radiation)
  const uint16_t caution = static_cast<uint16_t>(RADIATION_CRITICAL_USV_DAY *
                                                 RAD_CAUTION_PERCENT / PERCENT);
  CHECK(hazard_radiation_reasons(t_radiation{}) == 0);
  CHECK(hazard_radiation_reasons(t_radiation{0, caution, false, true}) == 0);
  CHECK(hazard_radiation_reasons(t_radiation{0, static_cast<uint16_t>(caution + 1), false, true}) ==
        HAZARD_RADIATION_HIGH);
  CHECK(hazard_radiation_reasons(radiation_at_pressure(0)) == HAZARD_RADIATION_HIGH ||
        hazard_radiation_reasons(radiation_at_pressure(0)) == 0);
  t_radiation critical = radiation_at_pressure(0);
  critical.critical = true;
  CHECK(hazard_radiation_reasons(critical) == HAZARD_RADIATION_CRITICAL);

  // Hazard map
  t_grid grid = {};
  const t_coord goal = {JEZERO.lon_udeg + 3500000, JEZERO.lat_udeg + 3000000};
  CHECK(grid_init(&grid, JEZERO, goal)); // crosses the cliff at 79.7 deg E
  const uint32_t count = grid_cell_count(&grid);
  std::vector<uint8_t> levels(count);
  CHECK(hazard_map(&grid, 0, levels.data(), count - 1) == 0);
  CHECK(hazard_map(&grid, 0, nullptr, count) == 0);
  CHECK(hazard_map(&grid, 0, levels.data(), count) == count);
  uint32_t dangers = 0;
  for (uint32_t i = 0; i < count; i++)
    dangers += levels[i] == HAZARD_DANGER;
  CHECK(dangers > 0 && dangers < count);
  constexpr uint32_t MAP_SAMPLE = 997;
  for (uint32_t i = 0; i < count; i += MAP_SAMPLE)
    CHECK(levels[i] == hazard_assess(grid_cell_coord(&grid, i), 0).level);

  // Routes
  const int32_t tenth_deg = static_cast<int32_t>(UDEG_PER_DEG / 10);
  const int32_t one_deg = static_cast<int32_t>(UDEG_PER_DEG);
  const t_coord north = {JEZERO.lon_udeg, JEZERO.lat_udeg + tenth_deg};
  const t_coord walk[] = {JEZERO, north};
  const t_route_hazard r = hazard_route(walk, 2, 0, 0);
  CHECK(r.length_mm == coord_distance_mm(JEZERO, north)); // ~5.9 km
  CHECK(r.duration_s == r.length_mm / WALK_SPEED_MM_S);
  CHECK(r.dose_eq_usv == uint64_t{j.dose_eq_usv_day} * r.duration_s / MARS_SOL_S);
  CHECK(r.level == HAZARD_CAUTION && r.reasons == HAZARD_COARSE_TERRAIN);
  // 2 deg north (~118 km): beyond the exploration zone, longer than an EVA
  const t_coord trek[] = {JEZERO, {JEZERO.lon_udeg, JEZERO.lat_udeg + one_deg},
                          {JEZERO.lon_udeg, JEZERO.lat_udeg + 2 * one_deg}};
  const t_route_hazard far = hazard_route(trek, 3, 0, 0);
  CHECK((far.reasons & HAZARD_BEYOND_ZONE) && (far.reasons & HAZARD_EVA_TOO_LONG));
  CHECK(far.level == HAZARD_CAUTION);
  // The same trek at 1 mm/s (~1300 sols): over the 600 mSv career budget
  const t_route_hazard slow = hazard_route(trek, 3, 0, 1);
  CHECK((slow.reasons & HAZARD_DOSE_BUDGET) && slow.level == HAZARD_DANGER);
  CHECK(slow.dose_eq_usv > RAD_CAREER_LIMIT_USV);
  // Across the cliff
  const t_coord cross[] = {zone_cell_centre(JEZERO_ROW, CLIFF_COL - 2),
                           zone_cell_centre(JEZERO_ROW, CLIFF_COL + 1)};
  CHECK(hazard_route(cross, 2, 0, 0).level == HAZARD_DANGER);
  // Nothing to walk
  CHECK(hazard_route(nullptr, 0, 0, 0).length_mm == 0);
  CHECK(hazard_route(walk, 1, 0, 0).length_mm == 0);
}

// tools/fetch_landform.py, end to end //
// Synthetic image: elevation = (col % 300) * 100 + row % 100 (global indices)
constexpr uint32_t SYNTH_COL_PERIOD = 300;
constexpr int32_t SYNTH_COL_STEP_M = 100;
constexpr uint32_t SYNTH_ROW_PERIOD = 100;

// Layers read from files stay alive until the end of the tests
static std::vector<uint32_t> g_tool_blobs[2];

static uint32_t read_layer(const char *path, std::vector<uint32_t> *blob) {
  FILE *file = fopen(path, "rb");
  CHECK(file != nullptr);
  if (!file)
    return (0);
  fseek(file, 0, SEEK_END);
  const uint32_t size = static_cast<uint32_t>(ftell(file));
  rewind(file);
  blob->assign((size + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
  CHECK(fread(blob->data(), 1, size, file) == size);
  fclose(file);
  return (size);
}

static void test_tool_zone(const char *path) {
  const uint32_t size = read_layer(path, &g_tool_blobs[0]);
  CHECK(landform_init(g_tool_blobs[0].data(), size));

  const t_coord points[] = {JEZERO, {72000000, 14000000}, {82000000, 22000000}};
  for (const t_coord &p : points) {
    const uint32_t col = p.lon_udeg / LANDFORM_MOLA_STEP_UDEG;
    const uint32_t row =
        static_cast<uint32_t>(LAT_MAX_UDEG - p.lat_udeg) / LANDFORM_MOLA_STEP_UDEG;
    const t_landform l = landform_get(p);
    CHECK(l.valid);
    CHECK(l.elevation_m == static_cast<int16_t>((col % SYNTH_COL_PERIOD) * SYNTH_COL_STEP_M +
                                                row % SYNTH_ROW_PERIOD));
    // 100 m per cell of ~1.85 km: atan = 3.1 deg = 6 or 7 half degrees
    CHECK(l.slope_half_deg == 6 || l.slope_half_deg == 7);
  }
  CHECK(!landform_get({0, 0}).valid);
}

// CTX layer made by the tool from tests/make_synthetic_mola.py --ctx, over the
// MOLA zone of test_tool_zone
constexpr uint32_t SYNTH_CTX_STEP_UDEG = 337;
constexpr uint32_t SYNTH_CTX_WEST_UDEG = 77400000;
constexpr int32_t SYNTH_CTX_NORTH_UDEG = 18500000;
constexpr int32_t SYNTH_CTX_BASE_M = 1000;
constexpr uint32_t SYNTH_CTX_COL_PERIOD = 50;
constexpr int32_t SYNTH_CTX_COL_STEP_M = 2;
constexpr uint32_t SYNTH_CTX_HOLE_FIRST = 100;

static void test_tool_layers(const char *path) {
  const uint32_t size = read_layer(path, &g_tool_blobs[1]);
  CHECK(landform_add_layer(g_tool_blobs[1].data(), size));
  CHECK(landform_layer_count() == 2 && landform_layer(0)->step_udeg == SYNTH_CTX_STEP_UDEG);
  CHECK(landform_layer(0)->west_udeg == SYNTH_CTX_WEST_UDEG &&
        landform_layer(0)->north_udeg == SYNTH_CTX_NORTH_UDEG);
  const uint32_t col = (JEZERO.lon_udeg - SYNTH_CTX_WEST_UDEG) / SYNTH_CTX_STEP_UDEG;
  const t_landform fine = landform_get(JEZERO);
  CHECK(fine.valid && fine.layer == 0);
  CHECK(fine.elevation_m == SYNTH_CTX_BASE_M + static_cast<int32_t>(col % SYNTH_CTX_COL_PERIOD) *
                                                   SYNTH_CTX_COL_STEP_M);
  // 2 m per ~19 m cell eastward: atan(0.105) = 6 deg = 12 half degrees
  CHECK(fine.slope_half_deg >= 11 && fine.slope_half_deg <= 13);
  // The NaN hole falls back to MOLA
  const t_coord hole = {SYNTH_CTX_WEST_UDEG + (SYNTH_CTX_HOLE_FIRST + 5) * SYNTH_CTX_STEP_UDEG,
                        SYNTH_CTX_NORTH_UDEG - static_cast<int32_t>((SYNTH_CTX_HOLE_FIRST + 5) *
                                                                    SYNTH_CTX_STEP_UDEG)};
  CHECK(landform_get(hole).valid && landform_get(hole).layer == 1);
  CHECK(landform_get({72000000, 14000000}).layer == 1);
}

// v4 layers: elevation = base + raw * unit, bilinear sampling in mm
static void test_landform_units() {
  constexpr uint16_t SIDE = 4;
  constexpr uint32_t STEP = 1000;
  constexpr uint32_t WEST = 10000000;
  constexpr int32_t NORTH = 10000000;
  constexpr uint16_t UNIT_MM = 10; // centimetres
  constexpr int32_t BASE_M = -2500;
  const t_landform_header h = {LANDFORM_MAGIC, WEST, NORTH, STEP, SIDE, SIDE, LANDFORM_VERSION, UNIT_MM, BASE_M};
  const uint32_t count = SIDE * SIDE;
  const uint32_t size = sizeof(h) + count * LANDFORM_BYTES_PER_CELL;
  std::vector<uint32_t> blob((size + 3) / 4);
  uint8_t *bytes = reinterpret_cast<uint8_t *>(blob.data());
  memcpy(bytes, &h, sizeof(h));
  int16_t *elevation = reinterpret_cast<int16_t *>(bytes + sizeof(h));
  // A ramp east: 0, 1.5, 3, 4.5 m above the base; one hole
  for (uint32_t i = 0; i < count; i++)
    elevation[i] = static_cast<int16_t>((i % SIDE) * 150);
  elevation[SIDE * SIDE - 1] = LANDFORM_NODATA_M;
  CHECK(landform_init(blob.data(), size));
  const t_coord c00 = {WEST + STEP / 2, NORTH - static_cast<int32_t>(STEP / 2)};
  const t_landform land = landform_get(c00);
  CHECK(land.valid && land.elevation_m == BASE_M);
  const t_landform col1 = landform_get({WEST + STEP + STEP / 2, c00.lat_udeg});
  // -2498.5 m: half away from zero
  CHECK(col1.valid && col1.elevation_m == BASE_M + 1);
  int32_t mm = 0;
  CHECK(landform_sample_mm(c00, &mm) && mm == BASE_M * 1000);
  // Half way between the centres of columns 0 and 1: 0.75 m
  CHECK(landform_sample_mm({WEST + STEP, c00.lat_udeg}, &mm) && mm == BASE_M * 1000 + 750);
  // Next to the hole: the post under the point stands in for it
  const t_coord near_hole = {WEST + 3 * STEP + STEP / 4, NORTH - static_cast<int32_t>(3 * STEP + STEP / 4)};
  CHECK(!landform_get({WEST + 3 * STEP + STEP / 2, NORTH - static_cast<int32_t>(3 * STEP + STEP / 2)}).valid);
  CHECK(landform_sample_mm({WEST + 3 * STEP + STEP / 2, NORTH - static_cast<int32_t>(2 * STEP + STEP / 2)}, &mm));
  CHECK(!landform_sample_mm(near_hole, &mm));
  CHECK(!landform_sample_mm({WEST - 1, c00.lat_udeg}, &mm));
  // unit 0 = whole metres (v4 default)
  t_landform_header metres = h;
  metres.unit_mm = 0;
  metres.base_m = 0;
  memcpy(bytes, &metres, sizeof(metres));
  CHECK(landform_init(blob.data(), size));
  CHECK(landform_get({WEST + STEP + STEP / 2, c00.lat_udeg}).elevation_m == 150);
  // A unit or a base that could overflow the 32-bit millimetres is refused
  t_landform_header huge = h;
  huge.unit_mm = LANDFORM_MAX_UNIT_MM + 1;
  memcpy(bytes, &huge, sizeof(huge));
  CHECK(!landform_init(blob.data(), size));
  huge = h;
  huge.base_m = -LANDFORM_MAX_BASE_M - 1;
  memcpy(bytes, &huge, sizeof(huge));
  CHECK(!landform_init(blob.data(), size));
  // The extremes allowed still decode without overflow
  huge.base_m = LANDFORM_MAX_BASE_M;
  huge.unit_mm = LANDFORM_MAX_UNIT_MM;
  memcpy(bytes, &huge, sizeof(huge));
  CHECK(landform_init(blob.data(), size));
  CHECK(landform_sample_mm({WEST + 2 * STEP + STEP / 2, c00.lat_udeg}, &mm) &&
        mm == LANDFORM_MAX_BASE_M * 1000 + 300 * LANDFORM_MAX_UNIT_MM);
  landform_clear();
}

static void test_imagery() {
  constexpr uint16_t W = 3;
  constexpr uint16_t H = 2;
  constexpr uint32_t STEP = 2000;
  constexpr uint32_t WEST = 20000000;
  constexpr int32_t NORTH = 5000000;
  const t_imagery_header h = {IMAGERY_MAGIC, WEST, NORTH, STEP, W, H, IMAGERY_VERSION, 0, 0};
  const uint32_t exact = sizeof(h) + W * H;
  const uint32_t padded = (exact + 3) / 4 * 4;
  std::vector<uint32_t> blob(padded / 4);
  uint8_t *bytes = reinterpret_cast<uint8_t *>(blob.data());
  memcpy(bytes, &h, sizeof(h));
  const uint8_t grey[W * H] = {10, 20, IMAGERY_NODATA, 30, 40, 50};
  memcpy(bytes + sizeof(h), grey, sizeof(grey));
  imagery_clear();
  CHECK(!imagery_add_layer(blob.data(), exact - 1));
  CHECK(!imagery_add_layer(blob.data(), padded + 4));
  CHECK(imagery_add_layer(blob.data(), padded));
  CHECK(imagery_layer_count() == 1 && imagery_layer(0)->width == W);
  uint16_t g = 0;
  const t_coord p00 = {WEST + STEP / 2, NORTH - static_cast<int32_t>(STEP / 2)};
  CHECK(imagery_sample(p00, &g) && g == 10 * IMAGERY_GREY_ONE);
  // Half way to the east neighbour: 15
  CHECK(imagery_sample({WEST + STEP, p00.lat_udeg}, &g) && g == 15 * IMAGERY_GREY_ONE);
  // No data under the point: nothing, even with neighbours
  CHECK(!imagery_sample({WEST + 2 * STEP + STEP / 2, p00.lat_udeg}, &g));
  CHECK(!imagery_sample({WEST - 1, p00.lat_udeg}, &g));
  t_imagery_header bad = h;
  bad.magic = LANDFORM_MAGIC;
  memcpy(bytes, &bad, sizeof(bad));
  CHECK(!imagery_add_layer(blob.data(), padded));
  imagery_clear();
  CHECK(!imagery_sample(p00, &g));
}

// The MCD link: hour of the cell's local solar time, unobserved cells
static void test_weather_mcd() {
  constexpr uint16_t COLS = 2;
  constexpr uint16_t ROWS = 1;
  constexpr uint16_t WEST_COL = 77;
  constexpr uint16_t SOUTH_ROW = 108; // 18 N
  const t_weather_mcd_header h = {WEATHER_MCD_MAGIC, WEATHER_MCD_VERSION, WEATHER_MCD_HOURS, 0,
                                  WEST_COL, SOUTH_ROW, COLS, ROWS, 220, 0};
  std::vector<uint8_t> blob(sizeof(h) + WEATHER_MCD_HOURS * COLS * ROWS * sizeof(t_weather_mcd_cell));
  memcpy(blob.data(), &h, sizeof(h));
  for (uint32_t hour = 0; hour < WEATHER_MCD_HOURS; hour++)
    for (uint32_t col = 0; col < COLS; col++) {
      // Column 1 never observed; column 0: 2000 + 10 dK per hour
      const t_weather_mcd_cell c = {static_cast<uint16_t>(2000 + hour * 10), 6000, 500, 15,
                                    static_cast<uint8_t>(hour * 8), static_cast<uint8_t>(col == 0), 0};
      memcpy(blob.data() + sizeof(h) + (hour * COLS + col) * sizeof(c), &c, sizeof(c));
    }
  CHECK(!weather_mcd_set_blob(blob.data(), static_cast<uint32_t>(blob.size() - 1)));
  CHECK(weather_mcd_set_blob(blob.data(), static_cast<uint32_t>(blob.size())));
  CHECK(weather_mcd_solar_longitude_ddeg() == 220);
  const t_coord cell = {WEST_COL * UDEG_PER_DEG + UDEG_PER_DEG / 2, 18500000};
  // Mission time at which the cell centre reads 10:30 local solar time
  const uint32_t at = (10 * MARS_HOUR_S + MARS_HOUR_S / 2 + MARS_SOL_S - local_solar_time_s(cell, 0)) % MARS_SOL_S;
  weather_mcd_set_time(at);
  weather_set_link(weather_sat_mcd);
  const t_weather w = weather_get(cell);
  CHECK(w.valid && w.temperature_dk == 2105 && w.dust_x40 == 15);
  // Wind from 80/256 at 10 h, 88/256 at 11 h: half way
  CHECK(w.wind_from_x256 == 84);
  CHECK(!weather_get({(WEST_COL + 1) * UDEG_PER_DEG + UDEG_PER_DEG / 2, 18500000}).valid);
  CHECK(!weather_get({(WEST_COL + 2) * UDEG_PER_DEG + UDEG_PER_DEG / 2, 18500000}).valid);
  CHECK(weather_mcd_set_blob(nullptr, 0));
  weather_set_link(weather_sat_mcd);
  CHECK(!weather_get(cell).valid);
  weather_set_link(nullptr);
}

// Footprint hazard and forecast storms
static void test_storms_and_area() {
  load_zone();
  weather_set_link(nullptr);
  // Next to the cliff column: the cell itself is gentle, the area sees it
  const t_coord beside = zone_cell_centre(5, CLIFF_COL - 1);
  CHECK(!(hazard_assess(beside, 0).reasons & HAZARD_TOO_STEEP));
  CHECK(hazard_assess_area(beside, 0, LANDFORM_MOLA_STEP_UDEG * 60).reasons & HAZARD_TOO_STEEP);
  CHECK(!(hazard_assess_area(beside, 0, 1000).reasons & HAZARD_TOO_STEEP));

  const t_coord centre = zone_cell_centre(5, JEZERO_COL);
  t_storm storm = {centre, 10000, 0, 20000000, 1000, 1000 + 3600, 1000, 120, 0};
  CHECK(hazard_set_storms(&storm, 1) && hazard_storm_count() == 1);
  CHECK(!hazard_storm_at(centre, 999));
  CHECK(hazard_storm_at(centre, 1000) != nullptr);
  CHECK(hazard_assess(centre, 1000).level == HAZARD_DANGER);
  CHECK(hazard_assess(centre, 1000).reasons & HAZARD_DUST_STORM);
  CHECK(!(hazard_assess(centre, 900).reasons & HAZARD_DUST_STORM));
  // It drifts east 10 m/s: after 3000 s its centre is 30 km east
  const t_coord later = hazard_storm_centre(storm, 4000);
  const uint32_t moved = coord_distance_mm(centre, later);
  CHECK(moved > 29900000 && moved < 30100000 && later.lon_udeg > centre.lon_udeg);
  CHECK(!hazard_storm_at(centre, 1000 + 3600));
  // Some time of a window: before the storm, through it, after it
  CHECK(!hazard_storm_during(centre, 0, 999));
  CHECK(hazard_storm_during(centre, 0, 1000) != nullptr);
  CHECK(hazard_storm_during(centre, 900, 5000) != nullptr);
  CHECK(!hazard_storm_during(centre, 1000 + 3600, UINT32_MAX));
  CHECK(!hazard_storm_during(centre, 2000, 1500)); // empty window
  // 25 km east of its start: reached only once it has drifted there
  const t_coord east = hazard_storm_centre(storm, 1000 + 2500);
  CHECK(!hazard_storm_during(east, 1000, 1000 + 400));
  CHECK(hazard_storm_during(east, 1000, 1000 + 700) != nullptr);
  CHECK(hazard_storm_during(east, 1000 + 2400, 1000 + 2400) != nullptr);
  // Farther than it can drift within the window: never
  CHECK(!hazard_storm_during(zone_cell_centre(5, JEZERO_COL + 40), 1000, 1000 + 3599));
  t_storm bad = storm;
  bad.end_s = bad.start_s;
  CHECK(!hazard_set_storms(&bad, 1) && hazard_storm_count() == 1);
  bad = storm;
  bad.drift_north_mm_s = HAZARD_STORM_MAX_DRIFT_MM_S + 1;
  CHECK(!hazard_set_storms(&bad, 1));
  CHECK(hazard_set_storms(nullptr, 0) && hazard_storm_count() == 0);
  CHECK(!hazard_storm_at(centre, 1000));
}

// Ground (thermal inertia): header checks, pixels, the softest within a disc
static void test_ground() {
  constexpr uint16_t W = 4;
  constexpr uint16_t H = 3;
  constexpr uint32_t STEP = 2000; // ~118 m
  constexpr uint32_t WEST = 20000000;
  constexpr int32_t NORTH = 5000000;
  constexpr uint8_t FIRM = 50;   // 400 tiu
  constexpr uint8_t SOFT = 10;   // 80 tiu: danger
  constexpr uint8_t DUSTY = 20;  // 160 tiu: caution
  const t_ground_header h = {GROUND_MAGIC, WEST, NORTH, STEP, W, H, GROUND_VERSION, 0, 0};
  const uint32_t exact = sizeof(h) + W * H;
  const uint32_t padded = (exact + GROUND_ALIGN - 1) / GROUND_ALIGN * GROUND_ALIGN;
  std::vector<uint32_t> blob(padded / 4 + 1);
  uint8_t *bytes = reinterpret_cast<uint8_t *>(blob.data());
  memcpy(bytes, &h, sizeof(h));
  // Row 0: firm firm soft firm / row 1: firm dusty firm none / row 2: firm
  const uint8_t inertia[W * H] = {FIRM, FIRM, SOFT, FIRM, FIRM, DUSTY, FIRM, GROUND_NODATA,
                                  FIRM, FIRM, FIRM, FIRM};
  memcpy(bytes + sizeof(h), inertia, sizeof(inertia));
  CHECK(!ground_set(blob.data(), exact - 1));
  CHECK(!ground_set(blob.data(), padded + GROUND_ALIGN));
  CHECK(!ground_set(bytes + 1, exact));
  CHECK(ground_set(blob.data(), exact));
  CHECK(ground_set(blob.data(), padded));
  const auto centre = [](uint32_t row, uint32_t col) {
    return (t_coord{WEST + col * STEP + STEP / 2, NORTH - static_cast<int32_t>(row * STEP + STEP / 2)});
  };
  t_ground g = ground_get(centre(0, 2));
  CHECK(g.valid && g.inertia_tiu == SOFT * GROUND_TIU_PER_STEP);
  CHECK(!ground_get(centre(1, 3)).valid); // no data
  CHECK(!ground_get({WEST - 1, NORTH - 1}).valid);
  CHECK(!ground_get({WEST + 1, NORTH + 1}).valid);
  CHECK(!ground_get({WEST + W * STEP, NORTH - 1}).valid);
  CHECK(hazard_ground_reasons(centre(0, 2)) == HAZARD_SOFT_GROUND);
  CHECK(hazard_ground_reasons(centre(1, 1)) == HAZARD_DUSTY_GROUND);
  CHECK(hazard_ground_reasons(centre(2, 0)) == 0);

  // The softest within a disc: from the centre of (0, 0), the soft pixel's
  // near edge is 1.5 pixels (~176 m) east, the dusty one's corner nearer
  const uint32_t pixel_mm = static_cast<uint32_t>(uint64_t{STEP} * mm_per_udeg_lon_q16(NORTH) >> Q16_SHIFT);
  CHECK(ground_softest(centre(0, 0), 50000).inertia_tiu == FIRM * GROUND_TIU_PER_STEP);
  CHECK(ground_softest(centre(0, 0), pixel_mm * 3 / 2 - 2000).inertia_tiu == DUSTY * GROUND_TIU_PER_STEP);
  g = ground_softest(centre(0, 0), pixel_mm * 3 / 2 + 2000);
  CHECK(g.valid && g.inertia_tiu == SOFT * GROUND_TIU_PER_STEP);
  // The dusty pixel diagonally: its corner is ~0.7 pixel away, not 0.5
  CHECK(ground_softest(centre(0, 0), pixel_mm * 6 / 10).inertia_tiu == FIRM * GROUND_TIU_PER_STEP);
  CHECK(ground_softest(centre(0, 0), pixel_mm * 8 / 10).inertia_tiu == DUSTY * GROUND_TIU_PER_STEP);
  // West and north of the layer: the pixels within reach still count
  const t_coord west = {WEST - STEP / 4, centre(0, 0).lat_udeg};
  CHECK(!ground_get(west).valid);
  g = ground_softest(west, pixel_mm / 2);
  CHECK(g.valid && g.inertia_tiu == FIRM * GROUND_TIU_PER_STEP);
  CHECK(!ground_softest(west, pixel_mm / 5).valid);
  const t_coord north = {centre(0, 2).lon_udeg, NORTH + static_cast<int32_t>(STEP / 4)};
  g = ground_softest(north, pixel_mm / 2);
  CHECK(g.valid && g.inertia_tiu == SOFT * GROUND_TIU_PER_STEP);
  CHECK(!ground_softest(north, pixel_mm / 5).valid);
  // The largest radius: no overflow, every pixel within reach
  g = ground_softest(centre(1, 1), UINT32_MAX);
  CHECK(g.valid && g.inertia_tiu == SOFT * GROUND_TIU_PER_STEP);
  // The clearance assessment sees soft ground within 50 m, the point not
  const t_coord beside = {WEST + 2 * STEP - STEP / 5, centre(0, 2).lat_udeg}; // ~24 m west
  CHECK(!(hazard_assess(beside, 0).reasons & HAZARD_SOFT_GROUND));
  CHECK(hazard_assess_clearance(beside, 0).reasons & HAZARD_SOFT_GROUND);
  CHECK(hazard_assess_clearance(beside, 0).level == HAZARD_DANGER);

  t_ground_header bad = h;
  bad.magic = IMAGERY_MAGIC;
  memcpy(bytes, &bad, sizeof(bad));
  CHECK(!ground_set(blob.data(), exact) && !ground_get(centre(0, 0)).valid);
  bad = h;
  bad.version = GROUND_VERSION + 1;
  memcpy(bytes, &bad, sizeof(bad));
  CHECK(!ground_set(blob.data(), exact));
  bad = h;
  bad.width = 0;
  memcpy(bytes, &bad, sizeof(bad));
  CHECK(!ground_set(blob.data(), sizeof(h)));
  bad = h;
  bad.step_udeg = 0;
  memcpy(bytes, &bad, sizeof(bad));
  CHECK(!ground_set(blob.data(), exact));
  CHECK(ground_set(nullptr, 0) && !ground_get(centre(0, 0)).valid);
}

// v5 relief: the clearance slope (steepest within 50 m) read in place
static void test_landform_clearance() {
  uint32_t size = 0;
  std::vector<uint32_t> blob = make_blob(ZONE_HEADER, &size);
  const uint32_t count = uint32_t{ZONE_SIDE} * ZONE_SIDE;
  uint8_t *near_slope = reinterpret_cast<uint8_t *>(blob.data()) + sizeof(t_landform_header) +
                        count * (sizeof(int16_t) + sizeof(uint8_t));
  constexpr uint8_t GENTLE = 2;
  const uint32_t cell = JEZERO_ROW * ZONE_SIDE + JEZERO_COL;
  near_slope[cell] = CLIFF_SLOPE_HALF_DEG;
  // A v3/v4 sized blob (3 bytes per cell) is refused
  CHECK(!landform_init(blob.data(), sizeof(t_landform_header) + count * 3));
  CHECK(landform_init(blob.data(), size));
  weather_set_link(nullptr);
  hazard_set_spe_alert(false);
  const t_landform l = landform_get(JEZERO);
  CHECK(l.valid && l.near_half_deg == CLIFF_SLOPE_HALF_DEG && l.slope_half_deg < GENTLE + ZONE_GENTLE_MODULO);
  CHECK(landform_get(zone_cell_centre(JEZERO_ROW, JEZERO_COL + 1)).near_half_deg ==
        zone_slope(JEZERO_ROW, JEZERO_COL + 1));
  CHECK(!(hazard_assess(JEZERO, 0).reasons & HAZARD_TOO_STEEP));
  CHECK(hazard_assess_clearance(JEZERO, 0).reasons & HAZARD_TOO_STEEP);
  CHECK(!(hazard_assess_clearance(zone_cell_centre(JEZERO_ROW, JEZERO_COL + 1), 0).reasons & HAZARD_TOO_STEEP));
  // The elevation and the slope did not move with the third grid
  CHECK(l.elevation_m == zone_elevation(JEZERO_COL) && l.slope_half_deg == zone_slope(JEZERO_ROW, JEZERO_COL));
  load_zone();
}

int main(int argc, char **argv) {
  test_coord();
  test_endian_crc();
  test_fixed_math();
  test_protocol();
  test_chunk_roundtrip();
  test_landform();
  load_zone();
  test_weather_cache();
  test_weather_model();
  test_radiation();
  test_gathering();
  test_grid();
  test_landform_layers();
  test_weather_allocation();
  test_grid_prefetch();
  test_hazard();
  test_landform_units();
  test_imagery();
  test_weather_mcd();
  test_storms_and_area();
  test_ground();
  test_landform_clearance();
  if (argc > 1)
    test_tool_zone(argv[1]);
  if (argc > 2)
    test_tool_layers(argv[2]);
  if (g_failed)
    printf("%d / %d check(s) FAILED\n", g_failed, g_checks);
  else
    printf("all %d checks passed\n", g_checks);
  return (g_failed != 0);
}

// Compile time checks of the public contract //
static_assert(sizeof(t_coord) == 8 && sizeof(t_landform) == 6);
static_assert(sizeof(t_weather) == 10 && sizeof(t_radiation) == 6);
static_assert(sizeof(t_grid) <= 64, "a grid handle stays small");
static_assert(ZONE_SIDE <= LANDFORM_MAX_SIDE);
static_assert(CLIFF_SLOPE_HALF_DEG > HAZARD_SLOPE_DANGER_HALF_DEG);
static_assert(RAMP_SLOPE_HALF_DEG > HAZARD_SLOPE_CAUTION_HALF_DEG &&
              RAMP_SLOPE_HALF_DEG <= HAZARD_SLOPE_DANGER_HALF_DEG);
static_assert(ZONE_GENTLE_MODULO <= HAZARD_SLOPE_CAUTION_HALF_DEG);
static_assert(RAMP_COL + CLIFF_WIDTH < ZONE_SIDE && CLIFF_COL + CLIFF_WIDTH < RAMP_COL);
static_assert(CLIFF_COL + CLIFF_WIDTH < ZONE_SIDE && JEZERO_COL < CLIFF_COL);
static_assert(ZONE_BASE_M + ZONE_STEP_M * (ZONE_SIDE - 1) < 0, "zone below datum");
// 600 mSv career limit over a 500 sol stay, nothing received yet
static_assert(RADIATION_CRITICAL_USV_DAY == 1200);
static_assert(sizeof(t_hazard) == 6 && sizeof(t_route_hazard) == 16);
