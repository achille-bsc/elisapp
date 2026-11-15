#include "./weather_protocol.hpp"
#include "../core/endian.hpp"
#include <type_traits>

static void put_header(uint8_t *p, uint8_t type, uint16_t col, uint16_t row) {
  put_u16(p, WEATHER_MAGIC);
  p[WEATHER_OFF_VERSION] = WEATHER_VERSION;
  p[WEATHER_OFF_TYPE] = type;
  put_u16(p + WEATHER_OFF_COL, col);
  put_u16(p + WEATHER_OFF_ROW, row);
}

static bool check_header(const uint8_t *p, uint8_t type) {
  return (get_u16(p) == WEATHER_MAGIC &&
          p[WEATHER_OFF_VERSION] == WEATHER_VERSION &&
          p[WEATHER_OFF_TYPE] == type);
}

static bool check_crc(const uint8_t *frame, uint16_t len) {
  const uint16_t body = static_cast<uint16_t>(len - CRC16_SIZE);
  return (crc16(frame, body) == get_u16(frame + body));
}

static void put_crc(uint8_t *frame, uint16_t len) {
  const uint16_t body = static_cast<uint16_t>(len - CRC16_SIZE);
  put_u16(frame + body, crc16(frame, body));
}

static bool chunk_exists(uint16_t col, uint16_t row) {
  return (col < WEATHER_CHUNK_COLS && row < WEATHER_CHUNK_ROWS);
}

uint16_t weather_encode_request(uint16_t col, uint16_t row, uint8_t *out,
                                uint16_t cap) {
  if (!out || cap < WEATHER_REQUEST_SIZE)
    return (0);
  put_header(out, WEATHER_TYPE_REQUEST, col, row);
  put_crc(out, WEATHER_REQUEST_SIZE);
  return (WEATHER_REQUEST_SIZE);
}

bool weather_decode_request(const uint8_t *in, uint16_t len, uint16_t *col,
                            uint16_t *row) {
  if (!in || len != WEATHER_REQUEST_SIZE ||
      !check_header(in, WEATHER_TYPE_REQUEST) || !check_crc(in, len))
    return (false);
  *col = get_u16(in + WEATHER_OFF_COL);
  *row = get_u16(in + WEATHER_OFF_ROW);
  return (chunk_exists(*col, *row));
}

uint16_t weather_encode_chunk(const t_weather_chunk &chunk, uint8_t *out,
                              uint16_t cap) {
  if (!out || cap < WEATHER_RESPONSE_SIZE)
    return (0);
  put_header(out, WEATHER_TYPE_RESPONSE, chunk.col, chunk.row);
  put_u32(out + WEATHER_OFF_BODY, chunk.time_s);
  uint8_t *cell = out + WEATHER_HEADER_SIZE;
  for (const t_weather &c : chunk.cells) {
    // 0 K on the wire: no observation for this cell
    put_u16(cell + WEATHER_CELL_OFF_TEMP, c.valid ? c.temperature_dk : 0);
    put_u16(cell + WEATHER_CELL_OFF_PRESSURE, c.pressure_dpa);
    put_u16(cell + WEATHER_CELL_OFF_WIND, c.wind_cms);
    cell[WEATHER_CELL_OFF_DUST] = c.dust_x40;
    cell[WEATHER_CELL_OFF_WIND_FROM] = c.wind_from_x256;
    cell += WEATHER_CELL_WIRE_SIZE;
  }
  put_crc(out, WEATHER_RESPONSE_SIZE);
  return (WEATHER_RESPONSE_SIZE);
}

// A complete, intact response for a chunk that exists on Mars
bool weather_frame_is_valid(const uint8_t *frame, uint16_t len) {
  return (frame && len == WEATHER_RESPONSE_SIZE &&
          check_header(frame, WEATHER_TYPE_RESPONSE) && check_crc(frame, len) &&
          chunk_exists(weather_frame_col(frame), weather_frame_row(frame)));
}

uint16_t weather_frame_col(const uint8_t *frame) {
  return (get_u16(frame + WEATHER_OFF_COL));
}

uint16_t weather_frame_row(const uint8_t *frame) {
  return (get_u16(frame + WEATHER_OFF_ROW));
}

uint32_t weather_frame_time(const uint8_t *frame) {
  return (get_u32(frame + WEATHER_OFF_BODY));
}

t_weather weather_frame_cell(const uint8_t *frame, uint32_t index) {
  const uint8_t *cell =
      frame + WEATHER_HEADER_SIZE + index * WEATHER_CELL_WIRE_SIZE;
  const uint16_t temperature_dk = get_u16(cell + WEATHER_CELL_OFF_TEMP);
  return (t_weather{temperature_dk, get_u16(cell + WEATHER_CELL_OFF_PRESSURE),
                    get_u16(cell + WEATHER_CELL_OFF_WIND),
                    cell[WEATHER_CELL_OFF_DUST], cell[WEATHER_CELL_OFF_WIND_FROM],
                    temperature_dk != WEATHER_NO_OBSERVATION_DK, 0});
}

bool weather_decode_chunk(const uint8_t *in, uint16_t len,
                          t_weather_chunk *out) {
  if (!out || !weather_frame_is_valid(in, len))
    return (false);
  out->col = weather_frame_col(in);
  out->row = weather_frame_row(in);
  out->time_s = weather_frame_time(in);
  for (uint32_t i = 0; i < WEATHER_CHUNK_CELLS; i++)
    out->cells[i] = weather_frame_cell(in, i);
  return (true);
}

static_assert(WEATHER_REQUEST_SIZE == 10);
static_assert(WEATHER_RESPONSE_SIZE == 526);
static_assert(WEATHER_CHUNK_CELLS == 64);
static_assert(WEATHER_CHUNK_COLS == 45 && WEATHER_CHUNK_ROWS == 23);
static_assert(WEATHER_CHUNK_COLS < UINT16_MAX && WEATHER_CHUNK_ROWS < UINT16_MAX,
              "chunk ids are u16 on the wire");
static_assert(LON_MAX_UDEG % WEATHER_CELL_UDEG == 0, "cells must wrap Mars");
static_assert(WEATHER_CHUNK_UDEG % WEATHER_CELL_UDEG == 0);
static_assert(WEATHER_CELL_OFF_WIND_FROM + 1 == WEATHER_CELL_WIRE_SIZE,
              "the wind direction is the last byte of a cell");
static_assert(std::is_trivially_copyable_v<t_weather_chunk>);
static_assert(sizeof(t_weather) == 10, "no hidden padding in a cell");
static_assert(sizeof(t_weather_chunk) == 8 + WEATHER_CHUNK_CELLS * 10);
static_assert(WEATHER_MAGIC != 0, "magic 0 marks a free cache slot");
static_assert((WEATHER_MAGIC & BYTE_MASK) != 0,
              "first byte 0 marks a free cache slot");
