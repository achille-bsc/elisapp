#include "./grid.hpp"
#include "../config.hpp"
#include "../core/fixed_math.hpp"
#include "../core/geodesy.hpp"
#include "../weather/weather.hpp"

// Every chunk of the planet, one bit each
constexpr uint32_t CHUNK_COUNT = WEATHER_CHUNK_COLS * WEATHER_CHUNK_ROWS;
constexpr uint32_t BITS_PER_WORD = 32;
constexpr uint32_t CHUNK_WORDS = (CHUNK_COUNT + BITS_PER_WORD - 1) / BITS_PER_WORD;

struct s_chunk_set {
  uint32_t bits[CHUNK_WORDS];
} typedef t_chunk_set;

static int64_t max_i64(int64_t a, int64_t b) { return (a > b ? a : b); }

static int64_t clamp_i64(int64_t v, int64_t lo, int64_t hi) {
  return (v < lo ? lo : (v > hi ? hi : v));
}

static int64_t ceil_div(int64_t num, int64_t den) {
  return ((num + den - 1) / den);
}

// n cells in millimetres, from the exact 1/256 mm size
static int64_t cells_mm(const t_grid *grid, int64_t n) {
  return (div_round(n * grid->cell_q8, int64_t{1} << GRID_CELL_FRAC_SHIFT));
}

static t_coord midpoint(t_coord a, t_coord b) {
  const int64_t lat = (int64_t{a.lat_udeg} + b.lat_udeg) / 2;
  const int64_t lon = int64_t{a.lon_udeg} + lon_delta_udeg(a.lon_udeg, b.lon_udeg) / 2;
  return (t_coord{lon_wrap_udeg(lon), static_cast<int32_t>(lat)});
}

static void to_local(const t_grid *grid, t_coord coord, int64_t *x, int64_t *y) {
  const int64_t d_lon = lon_delta_udeg(grid->reference.lon_udeg, coord.lon_udeg);
  const int64_t d_lat = int64_t{coord.lat_udeg} - grid->reference.lat_udeg;
  *x = div_round(d_lon * grid->mm_per_udeg_lon_q16, Q16_ONE);
  *y = div_round(d_lat * grid->mm_per_udeg_lat_q16, Q16_ONE);
}

static t_coord to_geo(const t_grid *grid, int64_t x, int64_t y) {
  const int64_t lon = grid->reference.lon_udeg +
                      div_round(x * Q16_ONE, grid->mm_per_udeg_lon_q16);
  const int64_t lat = grid->reference.lat_udeg +
                      div_round(y * Q16_ONE, grid->mm_per_udeg_lat_q16);
  return (t_coord{lon_wrap_udeg(lon), static_cast<int32_t>(lat)});
}

// p + u * along + v * across, v = perp(u) = (-u.y, u.x)
static void move(const t_grid *grid, int64_t *x, int64_t *y, int64_t along,
                 int64_t across) {
  *x += div_round(grid->u_x_q30 * along - grid->u_y_q30 * across, Q30_ONE);
  *y += div_round(grid->u_y_q30 * along + grid->u_x_q30 * across, Q30_ONE);
}

bool grid_init(t_grid *grid, t_coord start, t_coord goal) {
  if (!grid || !coord_is_valid(start) || !coord_is_valid(goal))
    return (false);
  *grid = {};
  grid->reference = midpoint(start, goal);
  grid->mm_per_udeg_lat_q16 = MM_PER_UDEG_LAT_Q16;
  grid->mm_per_udeg_lon_q16 = mm_per_udeg_lon_q16(grid->reference.lat_udeg);
  if (grid->mm_per_udeg_lon_q16 == 0)
    return (false); // at a pole the local projection is undefined

  int64_t ax = 0, ay = 0, bx = 0, by = 0;
  to_local(grid, start, &ax, &ay);
  to_local(grid, goal, &bx, &by);
  const int64_t dx = bx - ax;
  const int64_t dy = by - ay;
  // Checked before squaring: dx * dx must not overflow
  if (dx > GRID_MAX_LENGTH_MM || -dx > GRID_MAX_LENGTH_MM ||
      dy > GRID_MAX_LENGTH_MM || -dy > GRID_MAX_LENGTH_MM)
    return (false);
  const uint32_t length = isqrt_u64(static_cast<uint64_t>(dx * dx + dy * dy));
  if (length < GRID_MIN_LENGTH_MM || length > GRID_MAX_LENGTH_MM)
    return (false);

  const int64_t extension = max_i64(
      GRID_CORRIDOR_MIN_MM, int64_t{length} * GRID_EXTENSION_PERMILLE / GRID_PERMILLE);
  const int64_t half_width = max_i64(
      GRID_CORRIDOR_MIN_MM, int64_t{length} * GRID_CORRIDOR_PERMILLE / GRID_PERMILLE);
  const int64_t span = (length + 2 * extension) << GRID_CELL_FRAC_SHIFT;
  // Rounded up so that span / cell stays <= 512, like the double maths of
  // marspath (span / (span / 512) == 512): same width
  const int64_t cell = max_i64(int64_t{GRID_MIN_CELL_MM} << GRID_CELL_FRAC_SHIFT,
                               ceil_div(span, GRID_MAX_SIDE));
  grid->length_mm = length;
  grid->cell_q8 = static_cast<uint32_t>(cell);
  grid->cell_mm = static_cast<uint32_t>(div_round(cell, int64_t{1} << GRID_CELL_FRAC_SHIFT));
  grid->width = static_cast<uint16_t>(ceil_div(span, cell) + 1);
  grid->height = static_cast<uint16_t>(
      ceil_div((2 * half_width) << GRID_CELL_FRAC_SHIFT, cell) + 1);
  grid->u_x_q30 = static_cast<int32_t>(div_round(dx * Q30_ONE, length));
  grid->u_y_q30 = static_cast<int32_t>(div_round(dy * Q30_ONE, length));

  // origin = start - u * E - v * ((height - 1) / 2 * C)
  int64_t ox = ax;
  int64_t oy = ay;
  move(grid, &ox, &oy, -extension, -cells_mm(grid, (grid->height - 1) / 2));
  grid->origin_x_mm = static_cast<int32_t>(ox);
  grid->origin_y_mm = static_cast<int32_t>(oy);
  grid->start_index = grid_index_of(grid, start);
  grid->goal_index = grid_index_of(grid, goal);
  return (true);
}

uint32_t grid_cell_count(const t_grid *grid) {
  return (uint32_t{grid->width} * grid->height);
}

// Centre of cell (i, j), also outside the grid (i or j = -1, width...)
static t_coord cell_centre(const t_grid *grid, int64_t i, int64_t j) {
  int64_t x = grid->origin_x_mm;
  int64_t y = grid->origin_y_mm;
  move(grid, &x, &y, cells_mm(grid, i), cells_mm(grid, j));
  return (to_geo(grid, x, y));
}

t_coord grid_cell_coord(const t_grid *grid, uint32_t index) {
  return (cell_centre(grid, index % grid->width, index / grid->width));
}

uint32_t grid_index_of(const t_grid *grid, t_coord coord) {
  int64_t x = 0;
  int64_t y = 0;
  to_local(grid, coord, &x, &y);
  // A far away point (other side of Mars) would overflow the Q30 products:
  // it is clamped to the grid anyway
  x = clamp_i64(x - grid->origin_x_mm, -INT32_MAX, INT32_MAX);
  y = clamp_i64(y - grid->origin_y_mm, -INT32_MAX, INT32_MAX);
  // Projections on u and v = perp(u)
  const int64_t along = div_round(x * grid->u_x_q30 + y * grid->u_y_q30, Q30_ONE);
  const int64_t across = div_round(y * grid->u_x_q30 - x * grid->u_y_q30, Q30_ONE);
  const int64_t i = clamp_i64(
      div_round(along << GRID_CELL_FRAC_SHIFT, grid->cell_q8), 0, grid->width - 1);
  const int64_t j = clamp_i64(
      div_round(across << GRID_CELL_FRAC_SHIFT, grid->cell_q8), 0, grid->height - 1);
  return (static_cast<uint32_t>(j * grid->width + i));
}

bool grid_neighbor(const t_grid *grid, uint32_t index, uint32_t k,
                   uint32_t *out) {
  if (k >= GRID_NEIGHBORS || index >= grid_cell_count(grid))
    return (false);
  const int64_t i = int64_t{index % grid->width} + GRID_STEPS[k].di;
  const int64_t j = int64_t{index / grid->width} + GRID_STEPS[k].dj;
  if (i < 0 || j < 0 || i >= grid->width || j >= grid->height)
    return (false);
  *out = static_cast<uint32_t>(j * grid->width + i);
  return (true);
}

uint32_t grid_distance_mm(const t_grid *grid, uint32_t a, uint32_t b) {
  const int64_t di = cells_mm(grid, int64_t{a % grid->width} - b % grid->width);
  const int64_t dj = cells_mm(grid, int64_t{a / grid->width} - b / grid->width);
  return (isqrt_u64(static_cast<uint64_t>(di * di + dj * dj)));
}

t_data_point grid_cell_data(const t_grid *grid, uint32_t index, uint32_t time_s,
                            e_data_type type) {
  return (data_gathering(grid_cell_coord(grid, index), time_s, type));
}

bool grid_cell_is_hazard(const t_grid *grid, uint32_t index, uint32_t time_s) {
  return (data_is_hazard(grid_cell_coord(grid, index), time_s));
}

// Chunks under every cell centre plus a ring of one cell around the grid: the
// pathfinding probes up to its safety margin past the cells. A chunk (~470 km)
// is far bigger than a cell (<= 3.5 km), so no chunk can slip between centres.
static uint32_t collect_chunks(const t_grid *grid, t_chunk_set *set) {
  *set = {};
  uint32_t count = 0;
  for (int64_t j = -1; j <= grid->height; j++)
    for (int64_t i = -1; i <= grid->width; i++) {
      const t_coord c = cell_centre(grid, i, j);
      if (!coord_is_valid(c))
        continue; // past a pole
      uint16_t col = 0;
      uint16_t row = 0;
      weather_chunk_of(c, &col, &row);
      const uint32_t bit = uint32_t{row} * WEATHER_CHUNK_COLS + col;
      const uint32_t mask = 1u << (bit % BITS_PER_WORD);
      if (!(set->bits[bit / BITS_PER_WORD] & mask)) {
        set->bits[bit / BITS_PER_WORD] |= mask;
        count++;
      }
    }
  return (count);
}

uint32_t grid_weather_chunk_count(const t_grid *grid) {
  t_chunk_set set;
  return (collect_chunks(grid, &set));
}

uint32_t grid_weather_prefetch(const t_grid *grid) {
  t_chunk_set set;
  const uint32_t count = collect_chunks(grid, &set);
  // Fetching more than the cache holds would evict the first chunks again
  if (count > weather_cache_slots())
    return (count);
  uint32_t missing = 0;
  for (uint32_t bit = 0; bit < CHUNK_COUNT; bit++)
    if (set.bits[bit / BITS_PER_WORD] & (1u << (bit % BITS_PER_WORD)))
      missing += !weather_fetch_chunk(static_cast<uint16_t>(bit % WEATHER_CHUNK_COLS),
                                      static_cast<uint16_t>(bit / WEATHER_CHUNK_COLS));
  return (missing);
}

static_assert(CHUNK_WORDS * BITS_PER_WORD >= CHUNK_COUNT, "one bit per chunk");
static_assert(sizeof(t_chunk_set) <= 256, "small enough for the stack");
static_assert(GRID_MIN_LENGTH_MM < GRID_MIN_CELL_MM);
// Largest grid: (span / cell) <= 512 rounded up, + 1 -> fits uint16 sides
static_assert(GRID_MAX_SIDE + 2 < UINT16_MAX);
// Local positions: half length + extension + half width stay in int32 mm
static_assert(int64_t{GRID_MAX_LENGTH_MM} / 2 +
                  int64_t{GRID_MAX_LENGTH_MM} * GRID_EXTENSION_PERMILLE /
                      GRID_PERMILLE +
                  int64_t{GRID_MAX_LENGTH_MM} * GRID_CORRIDOR_PERMILLE /
                      GRID_PERMILLE <
              INT32_MAX);
// Q30 products: |u| <= 2^30 times a position <= 2^31 stays in int64 twice
static_assert((int64_t{INT32_MAX} * Q30_ONE) < INT64_MAX / 2);
static_assert(int64_t{GRID_MAX_LENGTH_MM} * GRID_MAX_LENGTH_MM * 2 <
              INT64_MAX);
static_assert(sizeof(GRID_STEPS) / sizeof(GRID_STEPS[0]) == GRID_NEIGHBORS);
// Largest cell (1.8e9 mm span / 512) in 1/256 mm fits the uint32_t field
static_assert(uint64_t{GRID_MAX_LENGTH_MM} * 2 / GRID_MAX_SIDE
                  << GRID_CELL_FRAC_SHIFT <
              UINT32_MAX);
