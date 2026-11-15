#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/storage/IFileMapper.hpp"

namespace mars {

	/// Memory-maps the data files for the data module and sets its weather
	/// link. Exactly one, alive as long as the application.
	class DataSession final : public IFileMapper {
	public:
		/// Same contract as the data module's t_sat_link.
		using WeatherLink = std::uint16_t (*)(const std::uint8_t* request, std::uint16_t requestSize,
			std::uint8_t* response, std::uint16_t responseCapacity);

		explicit DataSession(WeatherLink link);
		DataSession(const DataSession&) = delete;
		DataSession& operator=(const DataSession&) = delete;
		~DataSession();

		/// False if the file is missing or refused (same for the others).
		[[nodiscard]] bool addReliefLayer(const char* path);

		[[nodiscard]] bool addImageryLayer(const char* path);

		[[nodiscard]] bool setGround(const char* path);

		[[nodiscard]] const void* mapFile(const char* path, std::uint32_t& size) override;

		/// Empties the weather cache: the link is asked again.
		void refreshWeather();

	private:
		struct Mapping {
			void* address;
			std::size_t size;
		};

		[[nodiscard]] bool addLayer(const char* path, bool (*add)(const void* blob, std::uint32_t size));

		/// Upper bound of one file, so the data module's 32-bit sizes hold.
		static constexpr std::size_t kMaxFileBytes = std::size_t{1} << 30U;

		WeatherLink link_;
		std::vector<Mapping> mappings_;
		std::vector<std::uint8_t> weatherCache_;
	};

}
