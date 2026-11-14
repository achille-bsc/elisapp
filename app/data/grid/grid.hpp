#pragma once

#include "../core/coord.hpp"
#include "../gathering.hpp"
#include <cstdint>

// Grid aligned on the start -> goal axis: cell for cell the same grid as the
// pathfinding (marspath CoarseGrid::configure with the GridParams defaults).
// Keep the GRID_* constants in sync with app/core/pathfinding.
//   L = distance start -> goal (local projection centred on their midpoint)
//   E = max(3 km, 0.1 L)            extension before start and after goal
//   C = max(100 m, (L + 2E) / 512)  cell size: 1.2 L / 512 once L >= 30 km
//   W = max(3 km, 0.25 L)           half width of the corridor
//   width = ceil((L + 2E) / C) + 1, height = ceil(2W / C) + 1
//   centre(i, j) = origin + u * i * C + v * j * C, u = axis, v = perp(u)
//   index = j * width + i
// Lazy: nothing is stored, every cell is read from the data module on demand.
// Local frame: x = east, y = north, in millimetres.

// Constants //
constexpr uint32_t GRID_MAX_SIDE = 512;
constexpr uint32_t GRID_MIN_CELL_MM = 100000;
constexpr uint32_t GRID_CORRIDOR_MIN_MM = 3000000;
constexpr uint32_t GRID_EXTENSION_PERMILLE = 100;
constexpr uint32_t GRID_CORRIDOR_PERMILLE = 250;
constexpr uint32_t GRID_PERMILLE = 1000;
// Below 1 m the pathfinding goes straight, with no grid
constexpr uint32_t GRID_MIN_LENGTH_MM = 1000;
// Keeps every local position in int32 millimetres
constexpr uint32_t GRID_MAX_LENGTH_MM = 1500000000;
constexpr uint32_t GRID_NEIGHBORS = 8;
// Cell size kept in 1/256 mm: no rounding drift over 512 cells
constexpr uint32_t GRID_CELL_FRAC_SHIFT = 8;

// Structures //
struct s_grid_step {
  int8_t di;
  int8_t dj;
} typedef t_grid_step;

// 8-connected neighbourhood, same order as marspath kGridSteps
inline constexpr t_grid_step GRID_STEPS[GRID_NEIGHBORS] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

struct s_grid {
  t_coord reference;            // projection centre: midpoint start / goal
  uint32_t mm_per_udeg_lon_q16; // local scale east, Q16
  uint32_t mm_per_udeg_lat_q16; // local scale north, Q16
  int32_t origin_x_mm;          // centre of cell 0
  int32_t origin_y_mm;
  int32_t u_x_q30; // unit axis start -> goal, Q30
  int32_t u_y_q30;
  uint32_t length_mm;
  uint32_t cell_mm;              // rounded, for the A* costs
  uint32_t cell_q8;              // exact cell size, 1/256 mm
  uint16_t width;
  uint16_t height;
  uint32_t start_index;
  uint32_t goal_index;
} typedef t_grid;

// Functions //
// False if a point is invalid, too close (< 1 m) or too far, or at a pole
bool grid_init(t_grid *grid, t_coord start, t_coord goal);
uint32_t grid_cell_count(const t_grid *grid);
// Centre of the cell (may be past a pole: then coord_is_valid() is false)
t_coord grid_cell_coord(const t_grid *grid, uint32_t index);
// Nearest cell, clamped to the grid
uint32_t grid_index_of(const t_grid *grid, t_coord coord);
// Neighbour in direction k (GRID_STEPS), false if outside the grid
bool grid_neighbor(const t_grid *grid, uint32_t index, uint32_t k,
                   uint32_t *out);
// Distance in millimetres between two cell centres (for the A* costs)
uint32_t grid_distance_mm(const t_grid *grid, uint32_t a, uint32_t b);
t_data_point grid_cell_data(const t_grid *grid, uint32_t index, uint32_t time_s,
                            e_data_type type);
// Weather chunks (8x8 deg) the trip touches: size of the weather cache,
//   std::vector<uint8_t> cache(weather_cache_bytes(grid_weather_chunk_count(&g)));
//   weather_set_cache(cache.data(), cache.size());
uint32_t grid_weather_chunk_count(const t_grid *grid);
// Fetches all of them before the A*. Returns the chunks NOT guaranteed in the
// cache: 0 = the A* never waits for the radio. If the cache is too small,
// nothing is fetched and the needed count is returned.
uint32_t grid_weather_prefetch(const t_grid *grid);
bool grid_cell_is_hazard(const t_grid *grid, uint32_t index, uint32_t time_s);
