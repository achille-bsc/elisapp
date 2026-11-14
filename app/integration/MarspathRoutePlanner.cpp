#include "integration/MarspathRoutePlanner.hpp"

#include <algorithm>
#include <cmath>

#include "data/data.hpp"
#include "integration/DataUnits.hpp"
#include "integration/Tuning.hpp"

namespace mars {

	namespace {

		constexpr std::int64_t kHalfTurnE7 = 1'800'000'000;
		constexpr std::int64_t kFullTurnE7 = 3'600'000'000;
		constexpr std::uint32_t kMmPerM = 1'000;

		/// marspath longitudes are signed, in (-180, 180] degrees.
		[[nodiscard]] marspath::GeoPoint toGeoPoint(t_position position) noexcept
		{
			const std::int64_t longitude = position.longitudeEastE7;
			return {position.latitudeE7, static_cast<std::int32_t>(longitude > kHalfTurnE7 ? longitude - kFullTurnE7 : longitude)};
		}

		[[nodiscard]] t_position toPosition(marspath::GeoPoint point) noexcept
		{
			const std::int64_t longitude = point.lon_e7;
			return {point.lat_e7, static_cast<std::uint32_t>(longitude < 0 ? longitude + kFullTurnE7 : longitude)};
		}

		[[nodiscard]] t_coord toCoord(marspath::GeoPoint point) noexcept
		{
			return coord_from_e7(point.lat_e7, point.lon_e7);
		}

		[[nodiscard]] marspath::PlannerParams makeParams() noexcept
		{
			marspath::PlannerParams params;
			params.planet_radius_m = static_cast<double>(MARS_RADIUS_MM) / kMmPerM;
			params.time_budget_ms = tuning::kPlanningBudgetMs;
			return params;
		}

		[[nodiscard]] RouteStatus toStatus(marspath::Status status) noexcept
		{
			switch (status) {
			case marspath::Status::Ok:
				return RouteStatus::Ok;
			case marspath::Status::StartInHazard:
				return RouteStatus::StartInHazard;
			case marspath::Status::GoalInHazard:
				return RouteStatus::GoalInHazard;
			case marspath::Status::NoPathFound:
				return RouteStatus::NoPath;
			default:
				return RouteStatus::Failed;
			}
		}

		[[nodiscard]] std::uint32_t toMetres(double value) noexcept
		{
			return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0, 4.0e9)));
		}

	}

	float MarspathRoutePlanner::Terrain::elevation(marspath::GeoPoint point) const
	{
		return static_cast<float>(landform_get(toCoord(point)).elevation_m);
	}

	bool MarspathRoutePlanner::RiskMap::isHazard(marspath::GeoPoint point) const
	{
		return data_is_hazard(toCoord(point), timeSeconds);
	}

	MarspathRoutePlanner::MarspathRoutePlanner()
		: planner_{risk_, terrain_, makeParams()}
	{
	}

	Route MarspathRoutePlanner::plan(t_position from, t_position to, std::uint32_t departureSeconds)
	{
		waypoints_.clear();

		// The data module's grid has the range limits of the planner and tells
		// which weather chunks the search will read: fetch them first, so the
		// search never waits for the satellite.
		t_grid grid{};
		if (!grid_init(&grid, mars::toCoord(from), mars::toCoord(to))) {
			RouteSummary summary{};
			summary.status = RouteStatus::TooFar;
			summary.level = HazardLevel::Danger;
			return {waypoints_, summary};
		}
		static_cast<void>(grid_weather_prefetch(&grid));

		risk_.timeSeconds = departureSeconds;
		const marspath::PlanResult result = planner_.plan(toGeoPoint(from), toGeoPoint(to));
		const RouteSummary summary = summarise(result, departureSeconds);
		if (result.ok()) {
			waypoints_.reserve(result.path.size());
			for (const marspath::Waypoint& waypoint : result.path) {
				waypoints_.push_back(toPosition(waypoint.position));
			}
		}
		return {waypoints_, summary};
	}

	RouteSummary MarspathRoutePlanner::summarise(const marspath::PlanResult& result, std::uint32_t departureSeconds) const
	{
		RouteSummary summary{};
		summary.status = toStatus(result.status);
		if (!result.ok()) {
			summary.level = HazardLevel::Danger;
			return summary;
		}

		std::vector<t_coord> coords;
		coords.reserve(result.path.size());
		for (const marspath::Waypoint& waypoint : result.path) {
			coords.push_back(toCoord(waypoint.position));
		}
		// Each point is assessed at its time of passage on foot.
		const t_route_hazard hazard =
			hazard_route(coords.data(), static_cast<std::uint32_t>(coords.size()), departureSeconds, WALK_SPEED_MM_S);

		summary.level = toHazardLevel(hazard.level);
		summary.reasons = hazard.reasons;
		summary.lengthM = toMetres(result.stats.length_m);
		summary.ascentM = toMetres(result.stats.ascent_m);
		summary.descentM = toMetres(result.stats.descent_m);
		summary.walkSeconds = hazard.duration_s;
		summary.roverSeconds = static_cast<std::uint32_t>(std::uint64_t{summary.lengthM} * kMmPerM / tuning::kRoverSpeedMmPerSecond);
		summary.doseMicroSievert = hazard.dose_eq_usv;
		return summary;
	}

}
