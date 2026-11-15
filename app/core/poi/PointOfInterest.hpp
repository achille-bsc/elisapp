#pragma once

#include <cstdint>

#include "core/gps/Position.hpp"

namespace mars {

	/// What a place is, for its icon on the map.
	enum class PlaceCategory : std::uint8_t {
		Feature,   ///< Named terrain: crater, valley, plain (IAU names).
		Hardware,  ///< Spacecraft or equipment left on the ground.
		Site,      ///< A landing or take-off site.
	};

	/// A named place shown on the map. The strings are null-terminated UTF-8 owned
	/// by the repository that provided the point.
	struct PointOfInterest {
		const char* name;
		const char* description;
		t_position position;
		PlaceCategory category;
		/// Photo file made by tools/fetch_site.py ("" if none) and its credit.
		const char* photo;
		const char* photoCredit;
	};

}
