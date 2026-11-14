#pragma once

#include <cstdint>

#include "core/gps/Position.hpp"

namespace mars {

	/// A scientific target proposed by mission control. The strings are
	/// null-terminated UTF-8 owned by the repository that provided the mission.
	struct Mission {
		const char* name;
		const char* type;
		const char* description;
		t_position destination;
		std::uint16_t durationMinutes;    ///< Work on site, travel excluded.
		std::uint8_t difficultyPercent;   ///< Of the work on site, 0 to 100.
	};

}
