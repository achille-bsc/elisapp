#pragma once

#include <cstdint>
#include <vector>

namespace mars {

	/// Owns the memory lent to the data module (relief layer, weather cache) and
	/// connects its weather link. Create exactly one, before anything reads the
	/// data module, and keep it alive as long as the application runs.
	class DataSession {
	public:
		/// Radio link to the weather satellite: same contract as the data
		/// module's t_sat_link.
		using WeatherLink = std::uint16_t (*)(const std::uint8_t* request, std::uint16_t requestSize,
			std::uint8_t* response, std::uint16_t responseCapacity);

		explicit DataSession(WeatherLink link);
		DataSession(const DataSession&) = delete;
		DataSession& operator=(const DataSession&) = delete;
		~DataSession();

		/// Loads the relief layer file made by tools/fetch_landform.py. False if
		/// the file is missing or refused: every place is then "unknown terrain".
		[[nodiscard]] bool loadLandform(const char* path);

	private:
		/// 32-bit words: the data module needs its blobs aligned on 4 bytes.
		std::vector<std::uint32_t> landform_;
		std::vector<std::uint8_t> weatherCache_;
	};

}
