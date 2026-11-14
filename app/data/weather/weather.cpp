#include "./weather.hpp"
#include <type_traits>

static t_sat_link g_link = nullptr;
// Raw satellite frames: the CRC is checked once on arrival and the cells are
// read in place, so there is no decode loop, no copy and no big stack buffer.
// A slot whose first byte is 0 has no valid magic: it is free.
static uint8_t g_cache[WEATHER_CACHE_SLOTS][WEATHER_RESPONSE_SIZE];
static uint8_t g_cache_next = 0; // round robin victim
// shortcut: cached chunks never expire, add a sol_time_s check when the
// satellite really changes its forecast over time

// Little endian helpers //
static void put_u16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v) {
  put_u16(p, static_cast<uint16_t>(v & 0xFFFF));
  put_u16(p + 2, static_cast<uint16_t>(v >> 16));
}

static uint16_t get_u16(const uint8_t *p) {
  return (static_cast<uint16_t>(p[0] | (p[1] << 8)));
}

static uint32_t get_u32(const uint8_t *p) {
  return (get_u16(p) | (static_cast<uint32_t>(get_u16(p + 2)) << 16));
}

static void put_header(uint8_t *p, uint8_t type, uint16_t col, uint16_t row) {
  put_u16(p, WEATHER_MAGIC);
  p[WEATHER_OFF_VERSION] = WEATHER_VERSION;
  p[WEATHER_OFF_TYPE] = type;
  put_u16(p + WEATHER_OFF_COL, col);
  put_u16(p + WEATHER_OFF_ROW, row);
}

static bool check_header(const uint8_t *p, uint8_t type) {
  return (get_u16(p) == WEATHER_MAGIC && p[WEATHER_OFF_VERSION] == WEATHER_VERSION &&
          p[WEATHER_OFF_TYPE] == type);
}

// Magic lives in the first bytes: zeroing the first one frees the slot
static void free_slot(uint8_t *slot) { slot[0] = 0; }

static bool check_crc(const uint8_t *frame, uint16_t len) {
  return (weather_crc16(frame, static_cast<uint16_t>(len - WEATHER_CRC_SIZE)) ==
          get_u16(frame + len - WEATHER_CRC_SIZE));
}

// A complete, intact response for a chunk that exists on Mars
static bool check_response(const uint8_t *in, uint16_t len) {
  return (len == WEATHER_RESPONSE_SIZE &&
          check_header(in, WEATHER_TYPE_RESPONSE) && check_crc(in, len) &&
          get_u16(in + WEATHER_OFF_COL) < WEATHER_CHUNK_COLS &&
          get_u16(in + WEATHER_OFF_ROW) < WEATHER_CHUNK_ROWS);
}

uint16_t weather_encode_request(uint16_t col, uint16_t row, uint8_t *out,
                                uint16_t cap) {
  if (cap < WEATHER_REQUEST_SIZE)
    return (0);
  put_header(out, WEATHER_TYPE_REQUEST, col, row);
  put_u16(out + WEATHER_OFF_BODY, weather_crc16(out, WEATHER_OFF_BODY));
  return (WEATHER_REQUEST_SIZE);
}

bool weather_decode_request(const uint8_t *in, uint16_t len, uint16_t *col,
                            uint16_t *row) {
  if (len != WEATHER_REQUEST_SIZE || !check_header(in, WEATHER_TYPE_REQUEST) ||
      !check_crc(in, len))
    return (false);
  *col = get_u16(in + WEATHER_OFF_COL);
  *row = get_u16(in + WEATHER_OFF_ROW);
  return (*col < WEATHER_CHUNK_COLS && *row < WEATHER_CHUNK_ROWS);
}

uint16_t weather_encode_chunk(const t_weather_chunk &chunk, uint8_t *out,
                              uint16_t cap) {
  if (cap < WEATHER_RESPONSE_SIZE)
    return (0);
  put_header(out, WEATHER_TYPE_RESPONSE, chunk.col, chunk.row);
  put_u32(out + WEATHER_OFF_BODY, chunk.sol_time_s);
  uint8_t *cell = out + WEATHER_HEADER_SIZE;
  for (const t_weather &c : chunk.cells) {
    put_u16(cell + WEATHER_CELL_OFF_TEMP, c.temperature_dk);
    put_u16(cell + WEATHER_CELL_OFF_PRESSURE, c.pressure_dpa);
    put_u16(cell + WEATHER_CELL_OFF_WIND, c.wind_cms);
    cell[WEATHER_CELL_OFF_DUST] = c.dust_x40;
    cell += WEATHER_CELL_WIRE_SIZE;
  }
  put_u16(out + WEATHER_RESPONSE_SIZE - WEATHER_CRC_SIZE,
          weather_crc16(out, WEATHER_RESPONSE_SIZE - WEATHER_CRC_SIZE));
  return (WEATHER_RESPONSE_SIZE);
}

// Writes into `out` only once the frame is fully validated
bool weather_decode_chunk(const uint8_t *in, uint16_t len,
                          t_weather_chunk *out) {
  if (!check_response(in, len))
    return (false);
  out->col = get_u16(in + WEATHER_OFF_COL);
  out->row = get_u16(in + WEATHER_OFF_ROW);
  out->sol_time_s = get_u32(in + WEATHER_OFF_BODY);
  const uint8_t *cell = in + WEATHER_HEADER_SIZE;
  for (t_weather &c : out->cells) {
    c.temperature_dk = get_u16(cell + WEATHER_CELL_OFF_TEMP);
    c.pressure_dpa = get_u16(cell + WEATHER_CELL_OFF_PRESSURE);
    c.wind_cms = get_u16(cell + WEATHER_CELL_OFF_WIND);
    c.dust_x40 = cell[WEATHER_CELL_OFF_DUST];
    c.valid = true;
    cell += WEATHER_CELL_WIRE_SIZE;
  }
  return (true);
}

void weather_set_link(t_sat_link link) {
  g_link = link;
  for (uint8_t (&slot)[WEATHER_RESPONSE_SIZE] : g_cache)
    free_slot(slot);
  g_cache_next = 0;
}

// Cached frame of a chunk, or one exchange with the satellite that writes its
// answer straight into the cache. nullptr if it fails.
static const uint8_t *get_chunk(uint16_t col, uint16_t row) {
  for (const uint8_t (&slot)[WEATHER_RESPONSE_SIZE] : g_cache)
    if (get_u16(slot) == WEATHER_MAGIC && get_u16(slot + WEATHER_OFF_COL) == col &&
        get_u16(slot + WEATHER_OFF_ROW) == row)
      return (slot);
  if (!g_link)
    return (nullptr);

  uint8_t req[WEATHER_REQUEST_SIZE];
  const uint16_t req_len = weather_encode_request(col, row, req, sizeof(req));
  uint8_t *slot = g_cache[g_cache_next];
  const uint16_t resp_len = g_link(req, req_len, slot, WEATHER_RESPONSE_SIZE);
  // Intact, and really the chunk we asked for (a link could answer another)
  if (!check_response(slot, resp_len) || get_u16(slot + WEATHER_OFF_COL) != col ||
      get_u16(slot + WEATHER_OFF_ROW) != row) {
    free_slot(slot); // the victim is lost, free it
    return (nullptr);
  }
  g_cache_next = static_cast<uint8_t>((g_cache_next + 1) % WEATHER_CACHE_SLOTS);
  return (slot);
}

t_weather get_weather(t_coord coord) {
  const t_weather none = {0, 0, 0, 0, false};
  if (!coord_is_valid(coord))
    return (none);
  // Latitude counted from the south pole so every chunk index is unsigned
  const uint32_t lat = static_cast<uint32_t>(coord.y + LAT_MAX_UDEG);
  const uint8_t *frame =
      get_chunk(static_cast<uint16_t>(coord.x / WEATHER_CHUNK_UDEG),
                static_cast<uint16_t>(lat / WEATHER_CHUNK_UDEG));
  if (!frame)
    return (none);
  const uint32_t cell_x = coord.x % WEATHER_CHUNK_UDEG / WEATHER_CELL_UDEG;
  const uint32_t cell_y = lat % WEATHER_CHUNK_UDEG / WEATHER_CELL_UDEG;
  const uint8_t *cell =
      frame + WEATHER_HEADER_SIZE +
      (cell_y * WEATHER_CHUNK_SIDE + cell_x) * WEATHER_CELL_WIRE_SIZE;
  return (t_weather{get_u16(cell + WEATHER_CELL_OFF_TEMP),
                    get_u16(cell + WEATHER_CELL_OFF_PRESSURE),
                    get_u16(cell + WEATHER_CELL_OFF_WIND),
                    cell[WEATHER_CELL_OFF_DUST], true});
}

// Compile time checks of the wire format and of the memory budget //
constexpr uint8_t CRC_CHECK[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
static_assert(weather_crc16(CRC_CHECK, sizeof(CRC_CHECK)) == 0x29B1,
              "CRC16-CCITT-FALSE check value");
static_assert(WEATHER_REQUEST_SIZE == 10);
static_assert(WEATHER_RESPONSE_SIZE == 462);
static_assert(WEATHER_CHUNK_CELLS == 64);
static_assert(WEATHER_CHUNK_COLS == 45 && WEATHER_CHUNK_ROWS == 23);
static_assert(WEATHER_CHUNK_COLS < 0xFFFF && WEATHER_CHUNK_ROWS < 0xFFFF,
              "chunk ids are u16 on the wire");
static_assert(LON_MAX_UDEG % WEATHER_CELL_UDEG == 0, "cells must wrap Mars");
static_assert(WEATHER_CHUNK_UDEG % WEATHER_CELL_UDEG == 0);
static_assert(WEATHER_CACHE_SLOTS > 0);
static_assert(std::is_trivially_copyable_v<t_weather_chunk>);
static_assert(sizeof(t_weather) == 8, "no hidden padding in a cell");
static_assert(sizeof(t_weather_chunk) == 8 + 64 * 8);
static_assert(WEATHER_MAGIC != 0, "magic 0 marks a free cache slot");
static_assert(WEATHER_CACHE_SLOTS * WEATHER_RESPONSE_SIZE <= 5 * 1024,
              "weather cache RAM budget");