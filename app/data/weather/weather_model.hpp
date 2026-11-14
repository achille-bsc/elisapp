#pragma once

#include "../core/coord.hpp"
#include "../link/weather_protocol.hpp"
#include <cstdint>

// Local weather estimate at a point and at a time, from the satellite cell:
//   1. altitude: the satellite gives the pressure at the datum (0 m), the
//      hydrostatic law brings it to the MOLA elevation of the point:
//        P = P_datum * exp(-z / H), H = scale height
//   2. time: the diurnal cycle (thermal tide for the pressure, day and night
//      for the temperature) moves the values from the observation time to
//      `time_s`, with the local solar time of the point:
//        V(t) = V(t_obs) + A * (cos(phase(t)) - cos(phase(t_obs)))
// The same model serves short and long trips: ask each point at its expected
// time of passage. Wind and dust are kept as observed.
//
// Time = mission seconds counted from a midnight on the 0 deg meridian (like
// the Mars Coordinated Time). Mean solar time: the equation of time (up to
// ~50 min on Mars) is ignored.

// Constants //
constexpr int32_t PRESSURE_TABLE_MIN_M = -9000;  // below MOLA min (-8206 m)
constexpr int32_t PRESSURE_TABLE_STEP_M = 1000;
constexpr uint32_t PRESSURE_TABLE_SIZE = 32;     // up to +22000 m
constexpr int32_t PRESSURE_TABLE_MAX_M =
    PRESSURE_TABLE_MIN_M +
    PRESSURE_TABLE_STEP_M * static_cast<int32_t>(PRESSURE_TABLE_SIZE - 1);
constexpr int32_t PERMILLE = 1000;

// Functions //
uint32_t local_solar_time_s(t_coord coord, uint32_t time_s);
// exp(-z / H) in Q16, z clamped to the table range
uint32_t pressure_ratio_q16(int32_t elevation_m);
// cos(phase(t)) - cos(phase(t_obs)) in Q15, in [-2, 2] * Q15_ONE
int32_t diurnal_delta_q15(uint32_t lst_s, uint32_t obs_lst_s, uint32_t peak_s);
// Invalid outside the landform zone or without satellite data
t_weather weather_estimate(t_coord coord, uint32_t time_s);
// Datum pressure (0 m) -> local pressure, without the diurnal cycle
uint16_t pressure_at_elevation_dpa(uint16_t datum_dpa, int32_t elevation_m);
