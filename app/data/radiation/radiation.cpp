#include "./radiation.hpp"
#include "../config.hpp"
#include "../core/fixed_math.hpp"
#include "../landform/landform.hpp"
#include "../weather/weather_model.hpp"

uint16_t radiation_dose_ugy_day(uint16_t pressure_dpa, int32_t solar_mv) {
  const int64_t dose_ngy =
      int64_t{RAD_DOSE_INTERCEPT_NGY_DAY} +
      int64_t{RAD_PRESSURE_SLOPE_NGY_DAY_DPA} *
          (int64_t{pressure_dpa} - RAD_PRESSURE_REF_DPA) +
      int64_t{RAD_SOLAR_SLOPE_NGY_DAY_MV} * solar_mv;
  if (dose_ngy <= 0)
    return (0);
  const int64_t dose_ugy = div_round(dose_ngy, NGY_PER_UGY);
  return (static_cast<uint16_t>(dose_ugy > UINT16_MAX ? UINT16_MAX : dose_ugy));
}

t_radiation radiation_at_pressure(uint16_t pressure_dpa) {
  t_radiation rad = {};
  rad.dose_ugy_day = radiation_dose_ugy_day(pressure_dpa, SOLAR_MODULATION_MV);
  const uint32_t eq = static_cast<uint32_t>(div_round(
      int64_t{rad.dose_ugy_day} * RAD_QUALITY_FACTOR_PERCENT, PERCENT));
  rad.dose_eq_usv_day = static_cast<uint16_t>(eq > UINT16_MAX ? UINT16_MAX : eq);
  rad.critical = rad.dose_eq_usv_day >= RADIATION_CRITICAL_USV_DAY;
  rad.valid = true;
  return (rad);
}

t_radiation radiation_from(const t_landform &land, const t_weather &local) {
  if (!land.valid)
    return (t_radiation{});
  // No satellite: standard datum pressure, still corrected by the elevation
  return (radiation_at_pressure(
      local.valid ? local.pressure_dpa
                  : pressure_at_elevation_dpa(DATUM_PRESSURE_DEFAULT_DPA,
                                              land.elevation_m)));
}

t_radiation radiation_estimate(t_coord coord, uint32_t time_s) {
  const t_landform land = landform_get(coord);
  if (!land.valid)
    return (t_radiation{});
  return (radiation_from(land, weather_estimate(coord, time_s)));
}

static_assert(sizeof(t_radiation) == 6);
static_assert(RAD_PRESSURE_SLOPE_NGY_DAY_DPA < 0, "more air, more shielding");
static_assert(RAD_SOLAR_SLOPE_NGY_DAY_MV < 0, "stronger sun, fewer GCR");
static_assert(RAD_QUALITY_FACTOR_PERCENT > PERCENT, "Sv >= Gy for GCR");
static_assert(RADIATION_CRITICAL_USV_DAY > 0);
// At the reference pressure and solar modulation the fit gives ~0.21 mGy/sol
// (Hassler 2014: 0.210 +- 0.040 mGy/day)
static_assert(RAD_DOSE_INTERCEPT_NGY_DAY +
                      RAD_SOLAR_SLOPE_NGY_DAY_MV * SOLAR_MODULATION_MV >
                  170000 &&
              RAD_DOSE_INTERCEPT_NGY_DAY +
                      RAD_SOLAR_SLOPE_NGY_DAY_MV * SOLAR_MODULATION_MV <
                  250000);
