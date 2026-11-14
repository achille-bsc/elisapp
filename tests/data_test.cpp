#include "data/gathering.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

// Usage: data_test [zone_file]
// With a zone file made by tools/fetch_landform.py from the synthetic image
// (elevation = (col % 300) * 100 + row % 100), also checks the tool end to end.

static int g_failed = 0;
static int g_link_calls = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
      g_failed++;                                                              \
    }                                                                          \
  } while (0)

// Links //
static uint16_t counting_link(const uint8_t *req, uint16_t req_len,
                              uint8_t *resp, uint16_t resp_cap) {
  g_link_calls++;
  return (weather_sat_sim(req, req_len, resp, resp_cap));
}

static uint16_t dead_link(const uint8_t *, uint16_t, uint8_t *, uint16_t) {
  return (0);
}

static uint16_t corrupt_link(const uint8_t *req, uint16_t req_len,
                             uint8_t *resp, uint16_t resp_cap) {
  const uint16_t len = weather_sat_sim(req, req_len, resp, resp_cap);
  if (len)
    resp[20] ^= 0xFF;
  return (len);
}

// Answers a chunk that was not asked for
static uint16_t wrong_chunk_link(const uint8_t *, uint16_t, uint8_t *resp,
                                 uint16_t resp_cap) {
  uint8_t other[WEATHER_REQUEST_SIZE];
  weather_encode_request(0, 0, other, sizeof(other));
  return (weather_sat_sim(other, sizeof(other), resp, resp_cap));
}

// Weather //
static const t_coord JEZERO = {77450900, 18444500};

static void test_protocol() {
  uint8_t req[WEATHER_REQUEST_SIZE];
  uint16_t col = 0;
  uint16_t row = 0;
  CHECK(weather_encode_request(21, 14, req, sizeof(req)) == sizeof(req));
  CHECK(weather_decode_request(req, sizeof(req), &col, &row));
  CHECK(col == 21 && row == 14);
  CHECK(weather_encode_request(1, 1, req, sizeof(req) - 1) == 0);

  uint8_t bad[WEATHER_REQUEST_SIZE];
  memcpy(bad, req, sizeof(bad));
  bad[4] ^= 1; // CRC no longer matches
  CHECK(!weather_decode_request(bad, sizeof(bad), &col, &row));
  memcpy(bad, req, sizeof(bad));
  bad[0] ^= 1; // wrong magic
  CHECK(!weather_decode_request(bad, sizeof(bad), &col, &row));
  CHECK(!weather_decode_request(req, sizeof(req) - 1, &col, &row));
  // Chunk id outside the planet
  weather_encode_request(WEATHER_CHUNK_COLS, 0, req, sizeof(req));
  CHECK(!weather_decode_request(req, sizeof(req), &col, &row));
}

static void test_chunk_roundtrip() {
  t_weather_chunk in = {};
  in.col = 3;
  in.row = 9;
  in.sol_time_s = 123456;
  for (uint16_t i = 0; i < WEATHER_CHUNK_CELLS; i++)
    in.cells[i] = {static_cast<uint16_t>(1500 + i), 6005, 1225, 60, true};
  in.cells[7] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFF, true}; // type limits survive

  uint8_t frame[WEATHER_RESPONSE_SIZE];
  t_weather_chunk out = {};
  CHECK(weather_encode_chunk(in, frame, sizeof(frame)) == sizeof(frame));
  CHECK(weather_decode_chunk(frame, sizeof(frame), &out));
  CHECK(out.col == 3 && out.row == 9 && out.sol_time_s == 123456);
  CHECK(out.cells[5].temperature_dk == 1505);
  CHECK(out.cells[5].pressure_dpa == 6005);
  CHECK(out.cells[5].wind_cms == 1225);
  CHECK(out.cells[5].dust_x40 == 60);
  CHECK(out.cells[7].temperature_dk == 0xFFFF && out.cells[7].dust_x40 == 0xFF);
  CHECK(weather_encode_chunk(in, frame, sizeof(frame) - 1) == 0);

  // A bad frame must not touch the destination
  t_weather_chunk untouched = {};
  untouched.col = 42;
  frame[100] ^= 0x10;
  CHECK(!weather_decode_chunk(frame, sizeof(frame), &untouched));
  CHECK(untouched.col == 42);
}

static void test_get_weather() {
  CHECK(!get_weather(JEZERO).valid); // no link registered yet

  weather_set_link(counting_link);
  g_link_calls = 0;
  const t_weather a = get_weather(JEZERO);
  CHECK(a.valid);
  CHECK(a.temperature_dk > 1000 && a.temperature_dk < 3200); // 100..320 K
  CHECK(a.pressure_dpa > 3000 && a.pressure_dpa < 12000);    // 300..1200 Pa
  CHECK(g_link_calls == 1);

  // Same 8x8 deg chunk: served from the cache
  get_weather({JEZERO.x + 1000000, JEZERO.y + 1000000});
  CHECK(g_link_calls == 1);
  // Neighbour chunk: one more exchange
  get_weather({JEZERO.x + 8000000, JEZERO.y});
  CHECK(g_link_calls == 2);
  get_weather(JEZERO);
  CHECK(g_link_calls == 2);

  // Fill the cache past its size: oldest chunks leave, nothing breaks
  for (uint32_t i = 0; i < 2u * WEATHER_CACHE_SLOTS; i++)
    CHECK(get_weather({i * WEATHER_CHUNK_UDEG, 0}).valid);
  CHECK(get_weather(JEZERO).valid);

  // Planet edges
  CHECK(get_weather({0, -LAT_MAX_UDEG}).valid);
  CHECK(get_weather({LON_MAX_UDEG - 1, LAT_MAX_UDEG}).valid);
  CHECK(!get_weather({LON_MAX_UDEG, 0}).valid);

  weather_set_link(dead_link);
  CHECK(!get_weather(JEZERO).valid);
  weather_set_link(corrupt_link);
  CHECK(!get_weather(JEZERO).valid);
  weather_set_link(wrong_chunk_link);
  CHECK(!get_weather(JEZERO).valid);
  weather_set_link(weather_sat_sim);
  CHECK(get_weather(JEZERO).valid);
}

// Landform //
// Builds a zone blob in memory: 300x300 cells from (75E, 25N),
// elevation = col * 10, slope = index % 200.
static std::vector<uint32_t> make_blob(t_landform_header header,
                                       uint32_t *size) {
  const uint32_t count = static_cast<uint32_t>(header.width) * header.height;
  *size = sizeof(header) + count * 3;
  std::vector<uint32_t> blob((*size + 3) / 4); // 4 bytes aligned like a flash
  uint8_t *bytes = reinterpret_cast<uint8_t *>(blob.data());
  memcpy(bytes, &header, sizeof(header));
  int16_t *elevation = reinterpret_cast<int16_t *>(bytes + sizeof(header));
  uint8_t *slope = bytes + sizeof(header) + count * 2;
  for (uint32_t i = 0; i < count; i++) {
    elevation[i] = static_cast<int16_t>((i % header.width) * 10);
    slope[i] = static_cast<uint8_t>(i % 200);
  }
  return (blob);
}

static void test_landform() {
  const t_landform_header ok = {LANDFORM_MAGIC, 75000000, 25000000,
                                300,            300,      LANDFORM_VERSION,
                                0};
  CHECK(!get_landform(JEZERO).valid); // nothing loaded yet

  uint32_t size = 0;
  std::vector<uint32_t> blob = make_blob(ok, &size);
  CHECK(landform_init(blob.data(), size));
  const t_landform l = get_landform(JEZERO);
  CHECK(l.valid);
  CHECK(l.elevation_m == 780); // col (77.4509 - 75) * 32 = 78
  CHECK(l.slope_half_deg == (209 * 300 + 78) % 200); // row 209, col 78

  // Zone edges, inclusive on the north/west, exclusive on the south/east
  CHECK(get_landform({75000000, 25000000}).valid);
  CHECK(get_landform({75000000, 25000000}).elevation_m == 0);
  CHECK(!get_landform({74999999, 20000000}).valid);
  CHECK(!get_landform({80000000, 25000001}).valid);
  CHECK(!get_landform({80000000, 15625000}).valid);
  CHECK(!get_landform({84375000, 20000000}).valid);
  CHECK(!get_landform({LON_MAX_UDEG, 20000000}).valid);

  // Refused blobs leave nothing loaded
  CHECK(!landform_init(blob.data(), size - 1));                // truncated
  CHECK(!landform_init(blob.data(), size + 1));                // padded
  CHECK(!get_landform(JEZERO).valid);
  CHECK(!landform_init(nullptr, size));
  CHECK(!landform_init(blob.data(), 4));
  CHECK(!landform_init(reinterpret_cast<uint8_t *>(blob.data()) + 2, size - 2));
  t_landform_header bad = ok;
  bad.magic ^= 1;
  blob = make_blob(bad, &size);
  CHECK(!landform_init(blob.data(), size));
  bad = ok;
  bad.version = LANDFORM_VERSION + 1;
  blob = make_blob(bad, &size);
  CHECK(!landform_init(blob.data(), size));
  bad = ok;
  bad.width = 0;
  blob = make_blob(bad, &size);
  CHECK(!landform_init(blob.data(), size));
}

static void test_gathering() {
  uint32_t size = 0;
  const std::vector<uint32_t> blob = make_blob(
      {LANDFORM_MAGIC, 75000000, 25000000, 300, 300, LANDFORM_VERSION, 0},
      &size);
  CHECK(landform_init(blob.data(), size));
  const t_data_point land = data_gathering(JEZERO, LANDFORM);
  CHECK(land.landform.valid && !land.weather.valid);
  const t_data_point wx = data_gathering(JEZERO, WEATHER);
  CHECK(wx.weather.valid && !wx.landform.valid);
  const t_data_point temp = data_gathering(JEZERO, TEMPERATURE);
  CHECK(temp.weather.valid);
  const t_data_point rad = data_gathering(JEZERO, RADIATION);
  CHECK(!rad.weather.valid && !rad.landform.valid);
}

// Zone made by the python tool from the synthetic image
static void test_tool_zone(const char *path) {
  FILE *file = fopen(path, "rb");
  CHECK(file != nullptr);
  if (!file)
    return;
  fseek(file, 0, SEEK_END);
  const uint32_t size = static_cast<uint32_t>(ftell(file));
  rewind(file);
  std::vector<uint32_t> blob((size + 3) / 4);
  CHECK(fread(blob.data(), 1, size, file) == size);
  fclose(file);
  CHECK(landform_init(blob.data(), size));

  const t_coord points[] = {JEZERO, {72000000, 14000000}, {82000000, 22000000}};
  for (const t_coord &p : points) {
    const uint32_t col = p.x / LANDFORM_STEP_UDEG;
    const uint32_t row =
        static_cast<uint32_t>(90000000 - p.y) / LANDFORM_STEP_UDEG;
    const t_landform l = get_landform(p);
    CHECK(l.valid);
    CHECK(l.elevation_m == static_cast<int16_t>((col % 300) * 100 + row % 100));
    // 100 m per cell of ~1.75 km: atan = 3.26 deg = 6.5 half degrees
    CHECK(l.slope_half_deg == 6 || l.slope_half_deg == 7);
  }
  CHECK(!get_landform({0, 0}).valid);
}

int main(int argc, char **argv) {
  test_protocol();
  test_chunk_roundtrip();
  test_get_weather();
  test_landform();
  test_gathering();
  if (argc > 1)
    test_tool_zone(argv[1]);
  printf(g_failed ? "%d check(s) FAILED\n" : "all checks passed\n", g_failed);
  return (g_failed != 0);
}
