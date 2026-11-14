#pragma once

#include <array>
#include <cstdint>

#include "core/environment/Hazard.hpp"
#include "core/route/Route.hpp"

// Texts shown to the crew. SI units, written as the SI brochure asks
// (a space between the number and the unit).
namespace mars::ui {

	using ShortText = std::array<char, 24>;
	using LongText = std::array<char, 160>;

	/// "850 m" below one kilometre, "12.4 km" above.
	[[nodiscard]] ShortText formatDistance(std::uint32_t metres) noexcept;

	/// "45 min", "2 h 05 min" or "3 d 4 h".
	[[nodiscard]] ShortText formatDuration(std::uint32_t seconds) noexcept;

	[[nodiscard]] const char* hazardLevelName(HazardLevel level) noexcept;

	/// The reasons as a sentence fragment: "steep slope, high radiation".
	/// Empty when there is nothing to report.
	[[nodiscard]] LongText formatHazardReasons(HazardReasons reasons) noexcept;

	/// Why no route is shown, for every status but Ok.
	[[nodiscard]] const char* routeStatusMessage(RouteStatus status) noexcept;

}
