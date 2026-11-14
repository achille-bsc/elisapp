#pragma once

#include "core/gps/Position.hpp"

namespace mars {

	/// A named place shown on the map. The strings are null-terminated UTF-8 owned
	/// by the repository that provided the point.
	struct PointOfInterest {
		const char* name;
		const char* description;
		t_position position;
	};

}
