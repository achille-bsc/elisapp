#pragma once

#include <cstdint>

#include "../core/gps/Position.hpp"
#include "./Format.hpp"

// Local Mean Solar Time of a place on Mars: the clock the crew lives by.
// 24 Mars hours per sol, the hour being 1/24 sol (~61.6 Earth minutes).

namespace mars::ui {

// Functions //
// "14:32 LMST" at `position` at mission time `time_s`
ShortText format_lmst(t_position position, uint32_t time_s);
// "-63 °C" from tenths of kelvin
ShortText format_temperature(uint16_t temperature_dk);

} // namespace mars::ui
