#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/weather/StormForecast.hpp"

namespace mars {

	/// Storm bulletin (assets/storm_forecast.json) given to the data module.
	class DataStormForecast final : public IStormForecast {
	public:
		/// Times count from `startSeconds`, in Mars hours. False: no storm.
		[[nodiscard]] bool load(const char* path, std::uint32_t startSeconds);

		[[nodiscard]] std::span<const StormView> at(std::uint32_t timeSeconds) override;
		[[nodiscard]] const char* source() const noexcept override { return source_; }

	private:
		static constexpr long kMaxFileBytes = 64L * 1024L;

		std::unique_ptr<std::string> text_;
		std::vector<const char*> names_;
		std::vector<StormView> views_;
		const char* source_{"No storm forecast."};
	};

}
