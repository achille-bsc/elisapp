#pragma once

#include <vector>

#include "core/poi/PointOfInterest.hpp"

namespace mars {

	/// Decodes the points-of-interest document (see assets/points_of_interest.json):
	///
	///   { "points_of_interest": [
	///       { "name": "...", "description": "...", "latitude": 18.44, "longitude": 77.45 } ] }
	///
	/// Latitude is in degrees north (-90 to 90), longitude in degrees east
	/// (0 to 360, or -180 to 180). Unknown members are ignored.
	class PointOfInterestJsonParser {
	public:
		/// Parses [begin, end) in place: the points keep pointers into that buffer,
		/// which must outlive them and have a writable `end`. On failure `points` is
		/// left empty and false is returned.
		[[nodiscard]] static bool parse(char* begin, char* end, std::vector<PointOfInterest>& points);
	};

}
