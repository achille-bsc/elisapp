#pragma once

#include <chrono>

#include "core/time/IMissionClock.hpp"

namespace mars::platform {

	/// Mission clock driven by the monotonic clock of the machine: it starts at
	/// `startSeconds` and runs `scale` times faster than real time (1 on the
	/// device, more to rehearse a traverse on a PC).
	class SteadyMissionClock final : public IMissionClock {
	public:
		explicit SteadyMissionClock(std::uint32_t startSeconds, std::uint32_t scale = 1) noexcept;

		[[nodiscard]] std::uint32_t nowSeconds() const noexcept override;

	private:
		std::chrono::steady_clock::time_point origin_;
		std::uint32_t startSeconds_;
		std::uint32_t scale_;
	};

}
