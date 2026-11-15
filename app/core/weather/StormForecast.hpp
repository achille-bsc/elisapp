#pragma once

#include <cstdint>
#include <span>

#include "core/gps/Position.hpp"

namespace mars {

	struct StormView {
		const char* name;
		t_position centre;          ///< Where it is (or will start) at that time.
		std::uint32_t radiusM;
		std::uint32_t startSeconds;  ///< Mission time.
		std::uint32_t endSeconds;
		bool active;                 ///< Under way at that time.
	};

	class IStormForecast {
	public:
		IStormForecast() = default;
		IStormForecast(const IStormForecast&) = delete;
		IStormForecast& operator=(const IStormForecast&) = delete;
		virtual ~IStormForecast() = default;

		/// Storms not yet ended; valid until the next call.
		[[nodiscard]] virtual std::span<const StormView> at(std::uint32_t timeSeconds) = 0;
		[[nodiscard]] virtual const char* source() const noexcept = 0;
	};

}
