#pragma once

#include <cstdint>

namespace mars {

	/// Mission time: seconds counted from a midnight on the 0 degree meridian,
	/// like the Mars Coordinated Time. It drives every time-dependent model
	/// (diurnal cycle of the weather, radiation, hazards).
	class IMissionClock {
	public:
		IMissionClock() = default;
		IMissionClock(const IMissionClock&) = delete;
		IMissionClock& operator=(const IMissionClock&) = delete;
		virtual ~IMissionClock() = default;

		[[nodiscard]] virtual std::uint32_t nowSeconds() const noexcept = 0;
	};

}
