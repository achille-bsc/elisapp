#pragma once

#include <cstdint>
#include <span>

#include "core/environment/Hazard.hpp"
#include "core/gps/Position.hpp"

namespace mars {

	enum class RouteStatus : std::uint8_t {
		Ok,
		StartInHazard,  ///< The crew stands in a danger zone.
		GoalInHazard,   ///< The destination is in a danger zone.
		NoPath,         ///< Every way to the destination crosses a danger zone.
		TooFar,         ///< The destination is out of the planning range.
		Failed,
	};

	/// What following a route costs and risks.
	struct RouteSummary {
		RouteStatus status;
		HazardLevel level;       ///< Worst level met on the way, at the time of passage.
		HazardReasons reasons;   ///< Every reason met on the way.
		std::uint32_t lengthM;
		std::uint32_t ascentM;
		std::uint32_t descentM;
		std::uint32_t walkSeconds;
		std::uint32_t roverSeconds;
		std::uint32_t doseMicroSievert;  ///< Received on foot along the way.
	};

	/// A planned path: `waypoints` go from the start to the destination, both
	/// included, and are empty unless `summary.status` is Ok.
	struct Route {
		std::span<const t_position> waypoints;
		RouteSummary summary;
	};

}
