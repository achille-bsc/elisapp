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
		std::uint16_t durationMinutes;
		std::uint16_t walkMinutes;
		std::uint16_t vehicleMinutes;
		std::uint8_t difficultyPercent;
	};

}
