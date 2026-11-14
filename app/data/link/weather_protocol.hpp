#pragma once

#include "../core/coord.hpp"
#include "../core/crc16.hpp"
#include <cstdint>

// Radio protocol of the (speculative) weather satellite orbiting Mars.
// The world is cut in chunks of 8x8 cells of 1 deg: one request = one chunk.
//
// Everything is fixed point: values are stored and sent as they are, the
// front divides by the unit to display them.
//
// Wire format, little endian, every frame ends with a CRC16-CCITT-FALSE:
//   request  : magic u16, version u8, type u8, col u16, row u16, crc u16
//   response : magic u16, version u8, type u8, col u16, row u16, time_s u32,
//              64 cells (temp u16, pressure u16, wind u16, dust u8), crc u16
// col counts 8 deg chunks east from 0 deg, row counts them north from 90 S.
// time_s is the observation time: mission seconds, see weather_model.hpp.

// Constants //
constexpr uint16_t WEATHER_MAGIC = 0x5758; // "WX"
constexpr uint8_t WEATHER_VERSION = 2;     // v2: pressure reduced to datum
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
constexpr uint16_t WEATHER_OFF_BODY = 8; // request CRC / response time_s
// Byte offsets in a cell
constexpr uint16_t WEATHER_CELL_OFF_TEMP = 0;
constexpr uint16_t WEATHER_CELL_OFF_PRESSURE = 2;
constexpr uint16_t WEATHER_CELL_OFF_WIND = 4;
constexpr uint16_t WEATHER_CELL_OFF_DUST = 6;

constexpr uint16_t WEATHER_TIME_SIZE = sizeof(uint32_t);
constexpr uint16_t WEATHER_CELL_WIRE_SIZE = WEATHER_CELL_OFF_DUST + 1;
constexpr uint16_t WEATHER_HEADER_SIZE = WEATHER_OFF_BODY + WEATHER_TIME_SIZE;
constexpr uint16_t WEATHER_REQUEST_SIZE = WEATHER_OFF_BODY + CRC16_SIZE;
constexpr uint16_t WEATHER_RESPONSE_SIZE =
    WEATHER_HEADER_SIZE + WEATHER_CHUNK_CELLS * WEATHER_CELL_WIRE_SIZE +
    CRC16_SIZE;

// Structures //
struct s_weather {
  uint16_t temperature_dk; // tenths of kelvin: 2135 = 213.5 K
  uint16_t pressure_dpa;   // tenths of pascal: 6105 = 610.5 Pa
  uint16_t wind_cms;       // cm/s: 1250 = 12.5 m/s
  uint8_t dust_x40;        // dust opacity (tau) * 40: 20 = 0.5
  bool valid;
} typedef t_weather;

struct s_weather_chunk {
  uint32_t time_s;
  uint16_t col;
  uint16_t row;
  t_weather cells[WEATHER_CHUNK_CELLS];
} typedef t_weather_chunk;

// Functions //
uint16_t weather_encode_request(uint16_t col, uint16_t row, uint8_t *out,
                                uint16_t cap);
bool weather_decode_request(const uint8_t *in, uint16_t len, uint16_t *col,
                            uint16_t *row);
uint16_t weather_encode_chunk(const t_weather_chunk &chunk, uint8_t *out,
                              uint16_t cap);
// Writes into `out` only once the frame is fully validated
bool weather_decode_chunk(const uint8_t *in, uint16_t len, t_weather_chunk *out);

// In place access to a response frame (the cache keeps raw frames)
bool weather_frame_is_valid(const uint8_t *frame, uint16_t len);
uint16_t weather_frame_col(const uint8_t *frame);
uint16_t weather_frame_row(const uint8_t *frame);
uint32_t weather_frame_time(const uint8_t *frame);
t_weather weather_frame_cell(const uint8_t *frame, uint32_t index);
