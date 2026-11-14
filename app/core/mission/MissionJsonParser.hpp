#pragma once

#include <vector>

#include "core/mission/Mission.hpp"

namespace mars {

	/// Decodes the missions document (see assets/missions.json):
	///
	///   { "missions": [
	///       { "name": "...", "type": "Geology", "description": "...",
	///         "latitude": 18.44, "longitude": 77.45,
	///         "duration_minutes": 30, "difficulty_percent": 60 } ] }
	///
	/// Latitude is in degrees north (-90 to 90), longitude in degrees east
	/// (0 to 360, or -180 to 180). Unknown members are ignored.
	class MissionJsonParser {
	public:
		/// Parses [begin, end) in place: the missions keep pointers into that
		/// buffer, which must outlive them and have a writable `end`. On failure
		/// `missions` is left empty and false is returned.
		[[nodiscard]] static bool parse(char* begin, char* end, std::vector<Mission>& missions);
	};

}
