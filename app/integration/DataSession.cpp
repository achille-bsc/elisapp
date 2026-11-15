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

	std::uint32_t DataSession::readLayer(const char* path, std::vector<std::uint32_t>& words)
	{
		words.clear();
		const auto close = [](std::FILE* file) { std::fclose(file); };
		const std::unique_ptr<std::FILE, decltype(close)> file{std::fopen(path, "rb"), close};
		if (!file || std::fseek(file.get(), 0, SEEK_END) != 0) {
			return 0;
		}
		const long size = std::ftell(file.get());
		if (size <= 0 || size > kMaxLandformBytes || std::fseek(file.get(), 0, SEEK_SET) != 0) {
			return 0;
		}
		const auto bytes = static_cast<std::size_t>(size);
		words.resize((bytes + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t));
		if (std::fread(words.data(), 1, bytes, file.get()) != bytes) {
			words.clear();
			return 0;
		}
		return static_cast<std::uint32_t>(bytes);
	}

	bool DataSession::loadLandform(const char* path)
	{
		// Every layer goes: the fine one would point into a freed buffer otherwise.
		landform_clear();
		fineLandform_.clear();
		const std::uint32_t bytes = readLayer(path, landform_);
		if (bytes == 0 || !landform_init(landform_.data(), bytes)) {
			landform_clear();
			landform_.clear();
			return false;
		}
		return true;
	}

	bool DataSession::addLandformLayer(const char* path)
	{
		// One fine layer at a time: replacing it must not leave a dangling one.
		if (!fineLandform_.empty()) {
			return false;
		}
		const std::uint32_t bytes = readLayer(path, fineLandform_);
		if (bytes == 0 || !landform_add_layer(fineLandform_.data(), bytes)) {
			fineLandform_.clear();
			return false;
		}
		return true;
	}

}
