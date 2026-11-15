#include "platform/SteadyMissionClock.hpp"

namespace mars::platform {

	SteadyMissionClock::SteadyMissionClock(std::uint32_t startSeconds, std::uint32_t scale) noexcept
		: origin_{std::chrono::steady_clock::now()}
		, startSeconds_{startSeconds}
		, scale_{scale}
	{
	}

	std::uint32_t SteadyMissionClock::nowSeconds() const noexcept
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - origin_);
		const auto scaledSeconds = static_cast<std::uint64_t>(elapsed.count()) * scale_ / 1'000;
		// Wraps after 136 years of mission time, like the data module's time_s.
		return static_cast<std::uint32_t>(startSeconds_ + scaledSeconds);
	}

}
