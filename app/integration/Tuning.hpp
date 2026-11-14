#pragma once

#include <cstdint>

// Settings of the glue between the data module, the pathfinding and the
// front. Sources: SOURCES.txt at the repo root.
namespace mars::tuning {

	/// Relief layer made by tools/fetch_landform.py (see `make data`).
	inline constexpr const char* kLandformPath = "data/landform_zone.bin";

	/// Weather cache given to the data module: 64 chunks of 8 x 8 degrees
	/// (30 KiB) hold the whole map on screen plus a route, so neither the map
	/// rendering nor the planner evicts the other's chunks.
	inline constexpr std::uint32_t kWeatherCacheSlots = 64;

	/// Pressurised rover cruise speed: NASA's Space Exploration Vehicle concept
	/// drives at about 10 km/h.
	inline constexpr std::uint32_t kRoverSpeedMmPerSecond = 2'778;

	/// The planner stops polishing the route after this long, so the screen
	/// never freezes for more than a moment.
	inline constexpr double kPlanningBudgetMs = 1'500.0;

}
