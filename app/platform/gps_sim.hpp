#pragma once

#include "../core/gps/Position.hpp"
#include "../core/gps/packet/MarsPositionPacket.hpp"
#include <cstdint>

// SIMULATED GPS receiver, PC only (like app/data/sim): it emits the CCSDS
// position packets of app/core/gps/packet, so the application decodes them
// exactly as it would decode a real receiver. It stands still at its fix, or
// walks along a route at a steady speed from a start time.

// Functions //
void gps_sim_init(mars::t_position fix);
// Walks `route` (start first) from `start_time_s`, then stays at its end
void gps_sim_follow(const mars::t_position *route, uint32_t count,
                    uint32_t speed_mm_s, uint32_t start_time_s);
// Stops where it is at `time_s`
void gps_sim_stop(uint32_t time_s);
mars::t_position gps_sim_position(uint32_t time_s);
// The packet a receiver would send at `time_s` (sequence count increases)
mars::PositionPacket::Buffer gps_sim_packet(uint32_t time_s);
