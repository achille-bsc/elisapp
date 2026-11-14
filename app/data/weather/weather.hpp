#pragma once

#include "../core/coord.hpp"
#include "../link/weather_protocol.hpp"
#include <cstdint>

// Raw weather from the satellite: one chunk per request, kept in a cache of
// raw frames so the pathfinding never spams the satellite.
// The values are the satellite's: pressure reduced to the datum (0 m), at the
// observation time. weather_model.hpp turns them into local estimates.
//
// Cache sizing: a trip touches grid_weather_chunk_count() chunks. Give the
// module that many slots (weather_set_cache) and prefetch them
// (grid_weather_prefetch): the A* then never waits for the radio.
// The storage is owned by the caller: the module itself never allocates.

// Constants //
// One slot = raw frame + last use stamp (least recently used eviction)
constexpr uint32_t WEATHER_SLOT_BYTES = WEATHER_RESPONSE_SIZE + sizeof(uint32_t);

// Structures //
// Radio link: sends `req`, writes the answer in `resp`, returns its length
// (0 = no answer). Blocking: see the report for asynchronous designs.
typedef uint16_t (*t_sat_link)(const uint8_t *req, uint16_t req_len,
                               uint8_t *resp, uint16_t resp_cap);

// Functions //
constexpr uint32_t weather_cache_bytes(uint32_t slots) {
  return (slots * WEATHER_SLOT_BYTES);
}
// Caller owned cache of bytes / WEATHER_SLOT_BYTES slots, any alignment.
// nullptr (or too small for one slot) = back to the built-in cache
// (WEATHER_CACHE_SLOTS). Flushes the cache. Keep the storage alive until
// another weather_set_cache() call.
bool weather_set_cache(uint8_t *storage, uint32_t bytes);
uint32_t weather_cache_slots();
// No link by default: weather_get() is invalid until one is registered.
void weather_set_link(t_sat_link link); // also flushes the cache
// Chunk (8x8 deg) holding a point
void weather_chunk_of(t_coord coord, uint16_t *col, uint16_t *row);
// Makes sure a chunk is cached (one exchange if not). False if it failed.
bool weather_fetch_chunk(uint16_t col, uint16_t row);
// Cell holding `coord`. `time_s` (optional) receives the observation time.
t_weather weather_get(t_coord coord, uint32_t *time_s = nullptr);
