#pragma once

#include <cstdint>

// Settings of the glue between the data module, the pathfinding and the
// front. Sources: SOURCES.txt at the repo root.
namespace mars::tuning {

	/// Relief layer made by tools/fetch_landform.py (see `make data`).
	inline constexpr const char* kLandformPath = "data/landform_zone.bin";

	/// Optional finer relief (CTX DTM) over it, see tools/fetch_landform.py --grid.
	inline constexpr const char* kFineLandformPath = "data/ctx_layer.bin";

	inline constexpr const char* kPointsOfInterestPath = "assets/points_of_interest.json";
	inline constexpr const char* kMissionsPath = "assets/missions.json";

	/// The simulated GPS starts at the Perseverance landing site (Octavia E.
	/// Butler Landing, see assets/points_of_interest.json), where the crew's
	/// base stands in this scenario.
	inline constexpr std::int32_t kBaseLatitudeE7 = 184'447'000;
	inline constexpr std::uint32_t kBaseLongitudeE7 = 774'508'000;

	/// The mission clock starts at this local mean solar time at the base:
	/// 08:00, the morning start of a surface work day.
	inline constexpr std::uint32_t kStartLocalHour = 8;

	/// A position packet is read this often (1 Hz, like a common GNSS receiver).
	inline constexpr std::uint32_t kGpsPeriodMs = 1'000;

	/// The weather and risk layers follow the time of day: redrawn this often.
	inline constexpr std::uint32_t kBackgroundRefreshMs = 60'000;

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
