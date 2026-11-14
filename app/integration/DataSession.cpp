#include "integration/DataSession.hpp"

#include <cstddef>
#include <cstdio>
#include <memory>

#include "data/data.hpp"
#include "integration/Tuning.hpp"

namespace mars {

	namespace {

		/// Upper bound on the relief file, to keep memory use predictable.
		constexpr long kMaxLandformBytes = 64L * 1024L * 1024L;

	}

	DataSession::DataSession(WeatherLink link)
		: weatherCache_(weather_cache_bytes(tuning::kWeatherCacheSlots))
	{
		landform_clear();
		weather_set_link(link);
		weather_set_cache(weatherCache_.data(), static_cast<std::uint32_t>(weatherCache_.size()));
	}

	DataSession::~DataSession()
	{
		// The data module must not keep pointers into the buffers freed here.
		landform_clear();
		weather_set_cache(nullptr, 0);
		weather_set_link(nullptr);
	}

	bool DataSession::loadLandform(const char* path)
	{
		landform_clear();
		landform_.clear();

		const auto close = [](std::FILE* file) { std::fclose(file); };
		const std::unique_ptr<std::FILE, decltype(close)> file{std::fopen(path, "rb"), close};
		if (!file || std::fseek(file.get(), 0, SEEK_END) != 0) {
			return false;
		}
		const long size = std::ftell(file.get());
		if (size <= 0 || size > kMaxLandformBytes || std::fseek(file.get(), 0, SEEK_SET) != 0) {
			return false;
		}
		const auto bytes = static_cast<std::size_t>(size);
		landform_.resize((bytes + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t));
		if (std::fread(landform_.data(), 1, bytes, file.get()) != bytes
			|| !landform_init(landform_.data(), static_cast<std::uint32_t>(bytes))) {
			landform_clear();
			landform_.clear();
			return false;
		}
		return true;
	}

}
