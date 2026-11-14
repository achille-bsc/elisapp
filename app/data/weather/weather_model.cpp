#include "./weather_model.hpp"
#include "../config.hpp"
#include "../core/fixed_math.hpp"
#include "../landform/landform.hpp"
#include "./weather.hpp"

struct s_pressure_table {
  uint32_t q16[PRESSURE_TABLE_SIZE];
} typedef t_pressure_table;

static consteval t_pressure_table make_pressure_table() {
  t_pressure_table table = {};
  for (uint32_t k = 0; k < PRESSURE_TABLE_SIZE; k++) {
    const double z = PRESSURE_TABLE_MIN_M +
                     PRESSURE_TABLE_STEP_M * static_cast<double>(k);
    table.q16[k] = static_cast<uint32_t>(
        ce_round(ce_exp(-z / MARS_SCALE_HEIGHT_M) * Q16_ONE));
  }
  return (table);
}

static constexpr t_pressure_table PRESSURE_TABLE = make_pressure_table();

static uint16_t clamp_u16(int64_t v) {
  return (static_cast<uint16_t>(v < 0 ? 0 : (v > UINT16_MAX ? UINT16_MAX : v)));
}

uint32_t local_solar_time_s(t_coord coord, uint32_t time_s) {
  // 1 deg of longitude east = 1/360 sol later
  const uint32_t offset_s = static_cast<uint32_t>(
      uint64_t{coord.lon_udeg} * MARS_SOL_S / LON_MAX_UDEG);
  return ((time_s % MARS_SOL_S + offset_s) % MARS_SOL_S);
}

uint32_t pressure_ratio_q16(int32_t elevation_m) {
  int32_t z = elevation_m;
  if (z < PRESSURE_TABLE_MIN_M)
    z = PRESSURE_TABLE_MIN_M;
  if (z > PRESSURE_TABLE_MAX_M)
    z = PRESSURE_TABLE_MAX_M;
  const uint32_t from_min = static_cast<uint32_t>(z - PRESSURE_TABLE_MIN_M);
  const uint32_t step = static_cast<uint32_t>(PRESSURE_TABLE_STEP_M);
  const uint32_t index = from_min / step;
  if (index >= PRESSURE_TABLE_SIZE - 1)
    return (PRESSURE_TABLE.q16[PRESSURE_TABLE_SIZE - 1]);
  // The table decreases with z: interpolate on the drop
  const uint32_t drop = PRESSURE_TABLE.q16[index] - PRESSURE_TABLE.q16[index + 1];
  return (PRESSURE_TABLE.q16[index] - drop * (from_min % step) / step);
}

uint16_t pressure_at_elevation_dpa(uint16_t datum_dpa, int32_t elevation_m) {
  return (clamp_u16(
      (int64_t{datum_dpa} * pressure_ratio_q16(elevation_m)) >> Q16_SHIFT));
}

int32_t diurnal_delta_q15(uint32_t lst_s, uint32_t obs_lst_s, uint32_t peak_s) {
  // Phase 0 at the peak hour: cos = 1 at the daily maximum
  const uint32_t now = (lst_s + MARS_SOL_S - peak_s) % MARS_SOL_S;
  const uint32_t then = (obs_lst_s + MARS_SOL_S - peak_s) % MARS_SOL_S;
  return (periodic_cos_q15(now, MARS_SOL_S) -
          periodic_cos_q15(then, MARS_SOL_S));
}

t_weather weather_estimate(t_coord coord, uint32_t time_s) {
  const t_weather none = {};
  uint32_t obs_time_s = 0;
  const t_weather sat = weather_get(coord, &obs_time_s);
  const t_landform land = landform_get(coord);
  if (!sat.valid || !land.valid)
    return (none);

  const uint32_t lst = local_solar_time_s(coord, time_s);
  const uint32_t obs_lst = local_solar_time_s(coord, obs_time_s);
  const int64_t pressure =
      pressure_at_elevation_dpa(sat.pressure_dpa, land.elevation_m);
  const int64_t tide =
      diurnal_delta_q15(lst, obs_lst, PRESSURE_PEAK_HOUR * MARS_HOUR_S);
  const int64_t day =
      diurnal_delta_q15(lst, obs_lst, TEMPERATURE_PEAK_HOUR * MARS_HOUR_S);

  t_weather local = sat;
  local.pressure_dpa = clamp_u16(
      pressure + pressure * PRESSURE_DIURNAL_AMPLITUDE_PERMILLE * tide /
                     (int64_t{PERMILLE} * Q15_ONE));
  local.temperature_dk = clamp_u16(
      sat.temperature_dk + TEMPERATURE_DIURNAL_AMPLITUDE_DK * day / Q15_ONE);
  return (local);
}

// Datum = 1 (exp(0)): z = 0 m sits on the 10th entry
static_assert(PRESSURE_TABLE.q16[-PRESSURE_TABLE_MIN_M / PRESSURE_TABLE_STEP_M] ==
              Q16_ONE);
static_assert(PRESSURE_TABLE_MAX_M >= MOLA_MAX_M, "covers Olympus Mons");
static_assert(PRESSURE_TABLE_MIN_M <= MOLA_MIN_M, "covers Hellas");
// Highest ratio (table bottom, ~2.25): interpolation drop * step fits 32 bits
static_assert(PRESSURE_TABLE.q16[0] < 3 * Q16_ONE);
static_assert(uint64_t{PRESSURE_TABLE.q16[0]} * PRESSURE_TABLE_STEP_M <
              UINT32_MAX);
static_assert(MARS_SOL_S <= PERIOD_MAX, "periodic_cos_q15 range");
static_assert(PRESSURE_PEAK_HOUR < MARS_HOURS_PER_SOL);
static_assert(TEMPERATURE_PEAK_HOUR < MARS_HOURS_PER_SOL);
static_assert(PRESSURE_DIURNAL_AMPLITUDE_PERMILLE < PERMILLE);
