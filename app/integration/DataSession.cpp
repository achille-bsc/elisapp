#include "integration/DataSession.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "data/data.hpp"
#include "integration/Tuning.hpp"

namespace mars {

	DataSession::DataSession(WeatherLink link)
		: link_{link}
		, weatherCache_(weather_cache_bytes(tuning::kWeatherCacheSlots))
	{
		landform_clear();
		imagery_clear();
		weather_set_link(link_);
		weather_set_cache(weatherCache_.data(), static_cast<std::uint32_t>(weatherCache_.size()));
	}

	DataSession::~DataSession()
	{
		// The data module must not keep pointers into the pages unmapped here.
		landform_clear();
		imagery_clear();
		static_cast<void>(ground_set(nullptr, 0));
		weather_set_cache(nullptr, 0);
		weather_set_link(nullptr);
		for (const Mapping& mapping : mappings_) {
			munmap(mapping.address, mapping.size);
		}
	}

	const void* DataSession::mapFile(const char* path, std::uint32_t& size)
	{
		size = 0;
		const int fd = open(path, O_RDONLY | O_CLOEXEC);
		if (fd < 0) {
			return nullptr;
		}
		struct stat info{};
		void* address = MAP_FAILED;
		if (fstat(fd, &info) == 0 && info.st_size > 0 && static_cast<std::size_t>(info.st_size) <= kMaxFileBytes) {
			address = mmap(nullptr, static_cast<std::size_t>(info.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
		}
		close(fd);  // the mapping keeps the file
		if (address == MAP_FAILED) {
			return nullptr;
		}
		mappings_.push_back({address, static_cast<std::size_t>(info.st_size)});
		size = static_cast<std::uint32_t>(info.st_size);
		return address;
	}

	bool DataSession::addLayer(const char* path, bool (*add)(const void* blob, std::uint32_t size))
	{
		std::uint32_t size = 0;
		const void* const blob = mapFile(path, size);
		if (blob == nullptr) {
			return false;
		}
		if (!add(blob, size)) {
			munmap(mappings_.back().address, mappings_.back().size);
			mappings_.pop_back();
			return false;
		}
		return true;
	}

	bool DataSession::addReliefLayer(const char* path)
	{
		return addLayer(path, landform_add_layer);
	}

	bool DataSession::addImageryLayer(const char* path)
	{
		return addLayer(path, imagery_add_layer);
	}

	bool DataSession::setGround(const char* path)
	{
		return addLayer(path, ground_set);
	}

	void DataSession::refreshWeather()
	{
		weather_set_link(link_);  // flushes the cache
	}

}
