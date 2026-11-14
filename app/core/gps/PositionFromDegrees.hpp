#pragma once

#include <cmath>

#include "core/gps/Position.hpp"

namespace mars {

	/// Converts decimal degrees (latitude north in [-90, 90], longitude east in
	/// [0, 360] or [-180, 180]) to a position. False if out of range or NaN.
	[[nodiscard]] inline bool positionFromDegrees(double latitude, double longitude, t_position& position) noexcept
	{
		constexpr double kE7 = 1e7;
		constexpr std::int64_t kFullTurnE7 = 3'600'000'000;

		if (!(latitude >= -90.0 && latitude <= 90.0) || !(longitude >= -180.0 && longitude <= 360.0)) {
			return false;  // also rejects NaN
		}
		if (longitude < 0.0) {
			longitude += 360.0;
		}
		std::int64_t longitudeE7 = std::llround(longitude * kE7);
		if (longitudeE7 >= kFullTurnE7) {
			longitudeE7 -= kFullTurnE7;
		}
		position = {static_cast<std::int32_t>(std::llround(latitude * kE7)), static_cast<std::uint32_t>(longitudeE7)};
		return position.isValid();
	}

}
