#include "./weather.hpp"
#include "../config.hpp"
#include "../core/endian.hpp"

// Raw satellite frames: the CRC is checked once on arrival and the cells are
// read in place, so there is no decode loop, no copy and no big stack buffer.
// Slot layout: [frame][last use stamp u32]. A slot whose first byte is 0 has
// no valid magic: it is free.
static uint8_t g_builtin[WEATHER_CACHE_SLOTS * WEATHER_SLOT_BYTES];
static uint8_t *g_cache = g_builtin;
static uint32_t g_slots = WEATHER_CACHE_SLOTS;
static uint32_t g_clock = 0; // use stamps, 0 = never used
static t_sat_link g_link = nullptr;
// shortcut: cached chunks never expire, add a time_s check when the
// satellite really changes its forecast over time

static uint8_t *slot_at(uint32_t i) { return (g_cache + i * WEATHER_SLOT_BYTES); }

static uint8_t *stamp_of(uint8_t *slot) { return (slot + WEATHER_RESPONSE_SIZE); }

// The magic starts the frame: zeroing its first byte frees the slot
static void free_slot(uint8_t *slot) {
  slot[0] = 0;
  put_u32(stamp_of(slot), 0);
}

static void flush() {
  for (uint32_t i = 0; i < g_slots; i++)
    free_slot(slot_at(i));
  g_clock = 0;
}

static void touch(uint8_t *slot) {
  // On wrap (4e9 uses) the ages restart: the order is only lost once
  if (++g_clock == 0) {
    for (uint32_t i = 0; i < g_slots; i++)
      put_u32(stamp_of(slot_at(i)), 0);
    g_clock = 1;
  }
  put_u32(stamp_of(slot), g_clock);
}

static bool slot_holds(const uint8_t *slot, uint16_t col, uint16_t row) {
  return (get_u16(slot) == WEATHER_MAGIC && weather_frame_col(slot) == col &&
          weather_frame_row(slot) == row);
}

bool weather_set_cache(uint8_t *storage, uint32_t bytes) {
  const bool custom = storage && bytes >= WEATHER_SLOT_BYTES;
  g_cache = custom ? storage : g_builtin;
  g_slots = custom ? bytes / WEATHER_SLOT_BYTES : WEATHER_CACHE_SLOTS;
  flush();
  return (custom);
}

uint32_t weather_cache_slots() { return (g_slots); }

void weather_set_link(t_sat_link link) {
  g_link = link;
  flush();
}

void weather_chunk_of(t_coord coord, uint16_t *col, uint16_t *row) {
  // Latitude counted from the south pole so every chunk index is unsigned
  const uint32_t lat = static_cast<uint32_t>(coord.lat_udeg + LAT_MAX_UDEG);
  *col = static_cast<uint16_t>(coord.lon_udeg / WEATHER_CHUNK_UDEG);
  *row = static_cast<uint16_t>(lat / WEATHER_CHUNK_UDEG);
}

// Free slot first, else the least recently used one
static uint8_t *victim() {
  uint8_t *oldest = slot_at(0);
  for (uint32_t i = 0; i < g_slots; i++) {
    uint8_t *slot = slot_at(i);
    if (slot[0] == 0)
      return (slot);
    if (get_u32(stamp_of(slot)) < get_u32(stamp_of(oldest)))
      oldest = slot;
  }
  return (oldest);
}

// Cached frame of a chunk, or one exchange with the satellite that writes its
// answer straight into the cache. nullptr if it fails.
static const uint8_t *get_chunk(uint16_t col, uint16_t row) {
  for (uint32_t i = 0; i < g_slots; i++) {
    uint8_t *slot = slot_at(i);
    if (slot_holds(slot, col, row)) {
      touch(slot);
      return (slot);
    }
  }
  if (!g_link)
    return (nullptr);

  uint8_t req[WEATHER_REQUEST_SIZE];
  const uint16_t req_len = weather_encode_request(col, row, req, sizeof(req));
  uint8_t *slot = victim();
  const uint16_t resp_len = g_link(req, req_len, slot, WEATHER_RESPONSE_SIZE);
  // Intact, and really the chunk we asked for (a link could answer another)
  if (!weather_frame_is_valid(slot, resp_len) || !slot_holds(slot, col, row)) {
    free_slot(slot); // the victim is lost
    return (nullptr);
  }
  touch(slot);
  return (slot);
}

bool weather_fetch_chunk(uint16_t col, uint16_t row) {
  return (get_chunk(col, row) != nullptr);
}

t_weather weather_get(t_coord coord, uint32_t *time_s) {
  const t_weather none = {};
  if (!coord_is_valid(coord))
    return (none);
  uint16_t col = 0;
  uint16_t row = 0;
  weather_chunk_of(coord, &col, &row);
  const uint8_t *frame = get_chunk(col, row);
  if (!frame)
    return (none);
  const uint32_t lat = static_cast<uint32_t>(coord.lat_udeg + LAT_MAX_UDEG);
  const uint32_t cell_x = coord.lon_udeg % WEATHER_CHUNK_UDEG / WEATHER_CELL_UDEG;
  const uint32_t cell_y = lat % WEATHER_CHUNK_UDEG / WEATHER_CELL_UDEG;
  if (time_s)
    *time_s = weather_frame_time(frame);
  return (weather_frame_cell(frame, cell_y * WEATHER_CHUNK_SIDE + cell_x));
}

static_assert(WEATHER_CACHE_SLOTS > 0);
static_assert(weather_cache_bytes(WEATHER_CACHE_SLOTS) <= 8 * 1024,
              "built-in weather cache RAM budget");
static_assert(WEATHER_SLOT_BYTES == 466);
// The north pole (lat = +90 deg) falls in the partial last row
static_assert(2 * LAT_MAX_UDEG / WEATHER_CHUNK_UDEG < WEATHER_CHUNK_ROWS);
