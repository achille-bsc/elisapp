#pragma once

#include <cstdint>

namespace mars {

	/// Areographic position in degrees x 1e7. Longitude is positive east, 0 to 360.
	struct s_position {
		std::int32_t latitudeE7;
		std::uint32_t longitudeEastE7;

		[[nodiscard]] constexpr bool isValid() const noexcept
		{
			return latitudeE7 >= -900'000'000 && latitudeE7 <= 900'000'000 && longitudeEastE7 < 3'600'000'000u;
		}
	} typedef t_position;

}
