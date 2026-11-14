#pragma once

#include <cstdint>

// SIMULATED weather satellite, PC and tests only: leave sim/ out of a target
// build and register the real link with weather_set_link() instead.

// Functions //
// Same signature as t_sat_link: weather_set_link(weather_sat_sim)
uint16_t weather_sat_sim(const uint8_t *req, uint16_t req_len, uint8_t *resp,
                         uint16_t resp_cap);
// Observation time stamped on the next chunks (mission seconds, default 0)
void weather_sim_set_time(uint32_t time_s);
