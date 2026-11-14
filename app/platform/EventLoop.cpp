#include "platform/EventLoop.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

#include <lvgl.h>

namespace mars::platform {

	namespace {

		/// Longest sleep between two input polls, in milliseconds.
		constexpr std::uint32_t kMaxSleepMs = 33;

		std::uint32_t tickMs()
		{
			using Clock = std::chrono::steady_clock;
			static const Clock::time_point start = Clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
			return static_cast<std::uint32_t>(elapsed.count());
		}

	}

	void EventLoop::installTick()
	{
		lv_tick_set_cb(tickMs);
	}

	void EventLoop::run()
	{
		for (;;) {
			const std::uint32_t untilNextTimerMs = lv_timer_handler();
			std::this_thread::sleep_for(std::chrono::milliseconds{std::clamp<std::uint32_t>(untilNextTimerMs, 1, kMaxSleepMs)});
		}
	}

}
