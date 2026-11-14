#pragma once

#include "./coord.hpp"
#include <cstdint>

// Weather comes from a (speculative) weather satellite orbiting Mars.
// The world is cut in chunks of 8x8 cells of 1 deg: one request = one chunk,
// the answer is cached, so pathfinding never spams the satellite.
//
// Everything is fixed point (no float, no libm): the values are stored and
// sent as they are, the front divides by the unit to display them.
//
// Wire format, little endian, every frame ends with a CRC16-CCITT-FALSE:
//   request  : magic u16, version u8, type u8, col u16, row u16, crc u16
//   response : magic u16, version u8, type u8, col u16, row u16, sol_time u32,
//              64 cells (temp u16, pressure u16, wind u16, dust u8), crc u16

// Constants //
constexpr uint16_t WEATHER_MAGIC = 0x5758; // "WX"
constexpr uint8_t WEATHER_VERSION = 1;
constexpr uint8_t WEATHER_TYPE_REQUEST = 1;
constexpr uint8_t WEATHER_TYPE_RESPONSE = 2;

constexpr uint32_t WEATHER_CELL_UDEG = UDEG_PER_DEG; // 1 deg
constexpr uint32_t WEATHER_CHUNK_SIDE = 8;           // cells per chunk side
constexpr uint32_t WEATHER_CHUNK_UDEG = WEATHER_CELL_UDEG * WEATHER_CHUNK_SIDE;
constexpr uint32_t WEATHER_CHUNK_CELLS = WEATHER_CHUNK_SIDE * WEATHER_CHUNK_SIDE;
constexpr uint32_t WEATHER_CHUNK_COLS = LON_MAX_UDEG / WEATHER_CHUNK_UDEG;
// 180 deg is not a multiple of 8 deg: last row is partial, round up
constexpr uint32_t WEATHER_CHUNK_ROWS =
    (2 * LAT_MAX_UDEG + WEATHER_CHUNK_UDEG - 1) / WEATHER_CHUNK_UDEG;

// Byte offsets in a frame
constexpr uint16_t WEATHER_OFF_VERSION = 2;
constexpr uint16_t WEATHER_OFF_TYPE = 3;
constexpr uint16_t WEATHER_OFF_COL = 4;
constexpr uint16_t WEATHER_OFF_ROW = 6;
constexpr uint16_t WEATHER_OFF_BODY = 8; // request CRC / response sol_time
// Byte offsets in a cell
constexpr uint16_t WEATHER_CELL_OFF_TEMP = 0;
constexpr uint16_t WEATHER_CELL_OFF_PRESSURE = 2;
constexpr uint16_t WEATHER_CELL_OFF_WIND = 4;
constexpr uint16_t WEATHER_CELL_OFF_DUST = 6;

constexpr uint16_t WEATHER_CRC_SIZE = 2;
constexpr uint16_t WEATHER_SOL_TIME_SIZE = 4;
constexpr uint16_t WEATHER_CELL_WIRE_SIZE = WEATHER_CELL_OFF_DUST + 1;
constexpr uint16_t WEATHER_HEADER_SIZE =
    WEATHER_OFF_BODY + WEATHER_SOL_TIME_SIZE;
constexpr uint16_t WEATHER_REQUEST_SIZE = WEATHER_OFF_BODY + WEATHER_CRC_SIZE;
constexpr uint16_t WEATHER_RESPONSE_SIZE =
    WEATHER_HEADER_SIZE + WEATHER_CHUNK_CELLS * WEATHER_CELL_WIRE_SIZE +
    WEATHER_CRC_SIZE;

// CRC16-CCITT-FALSE
constexpr uint32_t CRC16_INIT = 0xFFFF;
constexpr uint32_t CRC16_MASK = 0xFFFF;
constexpr uint32_t CRC16_POLY = 0x1021;
constexpr uint32_t CRC16_TOP_BIT = 0x8000;
constexpr uint32_t BITS_PER_BYTE = 8;
// 500 km is ~17 deg: a 3x3 block of chunks covers a zone most of the time.
// A miss only costs one more exchange.
constexpr uint8_t WEATHER_CACHE_SLOTS = 9;

// Structures //
struct s_weather {
  uint16_t temperature_dk; // tenths of kelvin: 2135 = 213.5 K
  uint16_t pressure_dpa;   // tenths of pascal: 6105 = 610.5 Pa
  uint16_t wind_cms;       // cm/s: 1250 = 12.5 m/s
  uint8_t dust_x40;        // dust opacity (tau) * 40: 20 = 0.5
  bool valid;
} typedef t_weather;

struct s_weather_chunk {
  uint32_t sol_time_s;
  uint16_t col;
  uint16_t row;
  t_weather cells[WEATHER_CHUNK_CELLS];
} typedef t_weather_chunk;

// Radio link: sends `req`, writes the answer in `resp`, returns its length
// (0 = no answer).
typedef uint16_t (*t_sat_link)(const uint8_t *req, uint16_t req_len,
                               uint8_t *resp, uint16_t resp_cap);

// Functions //
constexpr uint16_t weather_crc16(const uint8_t *data, uint16_t len) {
  uint32_t crc = CRC16_INIT;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= static_cast<uint32_t>(data[i]) << BITS_PER_BYTE;
    for (uint32_t bit = 0; bit < BITS_PER_BYTE; bit++)
      crc = (crc & CRC16_TOP_BIT) ? (crc << 1) ^ CRC16_POLY : (crc << 1);
    crc &= CRC16_MASK;
  }
  return (static_cast<uint16_t>(crc));
}

uint16_t weather_encode_request(uint16_t col, uint16_t row, uint8_t *out,
                                uint16_t cap);
bool weather_decode_request(const uint8_t *in, uint16_t len, uint16_t *col,
                            uint16_t *row);
uint16_t weather_encode_chunk(const t_weather_chunk &chunk, uint8_t *out,
                              uint16_t cap);
bool weather_decode_chunk(const uint8_t *in, uint16_t len, t_weather_chunk *out);

// No link by default: get_weather() is invalid until one is registered.
void weather_set_link(t_sat_link link); // also flushes the cache
t_weather get_weather(t_coord);

// Simulated satellite. PC only: leave weather_sim.cpp out of a firmware build
// and register the real link instead.
uint16_t weather_sat_sim(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                         uint16_t resp_cap);
