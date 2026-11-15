#include "./mission_time.hpp"

#include <cstdio>

#include "../data/data.hpp"

namespace mars::ui {

// Constants //
constexpr uint32_t MINUTES_PER_HOUR = 60;
constexpr uint32_t LAST_HOUR = MARS_HOURS_PER_SOL - 1;
// 0 degrees Celsius = 273.15 K = 2731.5 tenths of kelvin
constexpr int32_t ZERO_CELSIUS_CK = 27315; // hundredths of kelvin
constexpr int32_t CK_PER_DK = 10;
constexpr int32_t CK_PER_DEGREE = 100;

ShortText format_lmst(t_position position, uint32_t time_s) {
  const uint32_t lst = local_solar_time_s(
      coord_from_e7_east(position.latitudeE7, position.longitudeEastE7), time_s);
  uint32_t hours = lst / MARS_HOUR_S;
  // The sol is not a whole number of seconds per hour: last seconds stay at 23
  if (hours > LAST_HOUR)
    hours = LAST_HOUR;
  const uint32_t minutes =
      (lst - hours * MARS_HOUR_S) * MINUTES_PER_HOUR / MARS_HOUR_S;
  ShortText text{};
  std::snprintf(text.data(), text.size(), "%02u:%02u LMST",
                static_cast<unsigned>(hours),
                static_cast<unsigned>(minutes < MINUTES_PER_HOUR ? minutes
                                                                 : MINUTES_PER_HOUR - 1));
  return (text);
}

ShortText format_temperature(uint16_t temperature_dk) {
  const int32_t centi =
      static_cast<int32_t>(temperature_dk) * CK_PER_DK - ZERO_CELSIUS_CK;
  // Rounded half away from zero to whole degrees
  const int32_t half = CK_PER_DEGREE / 2;
  const int32_t degrees =
      (centi >= 0 ? centi + half : centi - half) / CK_PER_DEGREE;
  ShortText text{};
  std::snprintf(text.data(), text.size(), "%d \xC2\xB0" "C", static_cast<int>(degrees));
  return (text);
}

static_assert(MARS_HOURS_PER_SOL * MARS_HOUR_S <= MARS_SOL_S);
static_assert(MARS_SOL_S - MARS_HOURS_PER_SOL * MARS_HOUR_S < MARS_HOUR_S);

} // namespace mars::ui
