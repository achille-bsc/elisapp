#pragma once

#include <cstdint>

// PC weather link answering from the Mars Climate Database file
// (data/weather_mcd.bin): 24 local hours per 1 deg cell, interpolated.
//   blob (little endian, read in place):
//     t_weather_mcd_header                     20 bytes
//     t_weather_mcd_cell [hours][rows][cols]   10 bytes, row 0 = south,
//                                              col 0 = west

// Constants //
constexpr uint32_t WEATHER_MCD_MAGIC = 0x5744434D; // "MCDW" little endian
constexpr uint16_t WEATHER_MCD_VERSION = 2; // v2: wind direction
constexpr uint8_t WEATHER_MCD_HOURS = 24;

// Structures //
struct s_weather_mcd_header {
  uint32_t magic;
  uint16_t version;
  uint8_t hours;
  uint8_t reserved;
  uint16_t west_col;  // 1 deg cells east of 0 E
  uint16_t south_row; // 1 deg cells north of 90 S
  uint16_t cols;
  uint16_t rows;
  uint16_t solar_longitude_ddeg; // season, tenths of degree
  uint16_t reserved2;
} typedef t_weather_mcd_header;

struct s_weather_mcd_cell {
  uint16_t temperature_dk;
  uint16_t pressure_dpa; // reduced to the datum (0 m)
  uint16_t wind_cms;
  uint8_t dust_x40;
  uint8_t wind_from_x256; // WMO: where it blows from, 1/256 turn from north
  uint8_t valid;
  uint8_t reserved;
} typedef t_weather_mcd_cell;

// Functions //
// Keeps the blob (nullptr: no data); it must stay alive while used.
bool weather_mcd_set_blob(const void *blob, uint32_t size);
// Observation time of the next chunks (mission seconds)
void weather_mcd_set_time(uint32_t time_s);
// Season of the data, tenths of degree of solar longitude (0 without data)
uint16_t weather_mcd_solar_longitude_ddeg();
// A t_sat_link
uint16_t weather_sat_mcd(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                         uint16_t resp_cap);
