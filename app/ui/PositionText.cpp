#include "ui/PositionText.hpp"

#include <cstdio>
#include <cstdlib>

namespace mars::ui {

	namespace {

		constexpr long long kE7 = 10'000'000;

		/// Keeps four decimals: about 6 m on the ground.
		constexpr long long kE7PerDisplayedUnit = 1'000;

	}

	PositionText formatPosition(t_position position) noexcept
	{
		const long long latitude = std::llabs(static_cast<long long>(position.latitudeE7));
		const long long longitude = position.longitudeEastE7;

		PositionText text{};
		std::snprintf(text.data(), text.size(), "%lld.%04lld° %c, %lld.%04lld° E",
			latitude / kE7, latitude % kE7 / kE7PerDisplayedUnit, position.latitudeE7 < 0 ? 'S' : 'N',
			longitude / kE7, longitude % kE7 / kE7PerDisplayedUnit);
		return text;
	}

}
