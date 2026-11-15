#pragma once

#include <cstdint>

#include "data/landform/landform.hpp"

// Application settings. Sources: SOURCES.txt.
namespace mars::tuning {

	inline constexpr const char* kMissionSitePath = "assets/mission_site.json";

	/// Data files of `make data`, finest first; missing ones are skipped.
	inline constexpr const char* kReliefPaths[] = {
		"data/relief_hirise.bin",  // HiRISE DTM, 2 m
		"data/relief_ctx.bin",     // CTX DTM, 20 m
		"data/relief_hrsc.bin",    // HRSC-MOLA blend, 200 m
		"data/relief_mola.bin",    // MOLA MEGDR, 1.85 km
	};

	inline constexpr const char* kImageryPaths[] = {
		"data/imagery_ctx.bin",     // CTX orthomosaic, 6 m
		"data/imagery_themis.bin",  // THEMIS Day IR, 100 m
	};

	inline constexpr const char* kGroundPath = "data/ground_ti.bin";

	inline constexpr const char* kWeatherPath = "data/weather_mcd.bin";

	/// Zone places and missions, else the curated ones of assets/.
	inline constexpr const char* kZonePointsOfInterestPath = "data/points_of_interest.json";
	inline constexpr const char* kZoneMissionsPath = "data/missions.json";
	inline constexpr const char* kPointsOfInterestPath = "assets/points_of_interest.json";
	inline constexpr const char* kMissionsPath = "assets/missions.json";

	inline constexpr const char* kStormForecastPath = "assets/storm_forecast.json";

	/// Fallback site: Octavia E. Butler Landing, 08:00, 3 km view.
	inline constexpr std::int32_t kBaseLatitudeE7 = 184'447'000;
	inline constexpr std::uint32_t kBaseLongitudeE7 = 774'508'000;

	inline constexpr std::uint32_t kStartLocalHour = 8;

	inline constexpr std::uint32_t kViewRadiusM = 3'000;

	/// 1 Hz, like a common GNSS receiver.
	inline constexpr std::uint32_t kGpsPeriodMs = 1'000;

	/// Time layers redrawn and weather asked again this often.
	inline constexpr std::uint32_t kBackgroundRefreshMs = 60'000;

	/// 64 chunks (30 KiB): the map and a route without evicting each other.
	inline constexpr std::uint32_t kWeatherCacheSlots = 64;

	/// Pressurised rover, ~10 km/h (NASA Space Exploration Vehicle).
	inline constexpr std::uint32_t kRoverSpeedMmPerSecond = 2'778;

	/// No danger within this distance of a planned path (team rule).
	inline constexpr std::uint32_t kClearanceMm = LANDFORM_CLEARANCE_M * 1'000;

	/// About one waypoint every 60 m (team choice).
	inline constexpr std::uint32_t kWaypointSpacingMm = 60'000;

	/// Coarse grid cell; finer cells are tried when no way is found.
	inline constexpr double kGridCellM = 30.0;

	inline constexpr double kFineGridCellM = 15.0;
	inline constexpr std::uint32_t kFineGridMaxSide = 1024;

	/// Path check step, far below the clearance.
	inline constexpr double kPathCheckStepM = 10.0;

	/// A storm blocks a point from the straight-line walking time to it,
	/// plus this share for detours and a margin.
	inline constexpr std::uint32_t kStormDetourPercent = 60;
	inline constexpr std::uint32_t kStormMarginSeconds = 1'800;

	/// Local optimisation budget.
	inline constexpr double kPlanningBudgetMs = 1'500.0;

}
