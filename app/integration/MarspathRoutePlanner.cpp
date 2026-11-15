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


		[[nodiscard]] t_coord toCoord(marspath::GeoPoint point) noexcept
		{
			return coord_from_e7(point.lat_e7, point.lon_e7);
		}

		[[nodiscard]] marspath::PlannerParams makeParams(bool fine) noexcept
		{
			marspath::PlannerParams params;
			params.planet_radius_m = static_cast<double>(MARS_RADIUS_MM) / kMmPerM;
			params.time_budget_ms = tuning::kPlanningBudgetMs;
			// The 50 m clearance is in the risk map: marspath's margin is off
			params.safety.margin_m = 0.0;
			params.safety.check_step_m = tuning::kPathCheckStepM;
			params.grid.min_cell_m = fine ? tuning::kFineGridCellM : tuning::kGridCellM;
			params.grid.max_side = fine ? tuning::kFineGridMaxSide : params.grid.max_side;
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
		return level(point) == marspath::RiskLevel::Danger;
	}

	marspath::RiskLevel MarspathRoutePlanner::RiskMap::level(marspath::GeoPoint point) const
	{
		const t_coord here = toCoord(point);
		// No 50 m clearance at the ends of the path
		const bool atEnd = coord_distance_mm(start, here) < tuning::kClearanceMm
			|| coord_distance_mm(goal, here) < tuning::kClearanceMm;
		// The crew may pass there from the straight-line time to a detour
		const std::uint64_t direct = coord_distance_mm(start, here) / WALK_SPEED_MM_S;
		const std::uint64_t from = timeSeconds + direct;
		const std::uint64_t until = from + direct * tuning::kStormDetourPercent / 100U + tuning::kStormMarginSeconds;
		return classify(here, static_cast<std::uint32_t>(std::min<std::uint64_t>(from, UINT32_MAX)),
			static_cast<std::uint32_t>(std::min<std::uint64_t>(until, UINT32_MAX)), atEnd);
	}

	marspath::RiskLevel MarspathRoutePlanner::RiskMap::classify(t_coord here, std::uint32_t fromSeconds,
		std::uint32_t untilSeconds, bool atEnd) const
	{
		const t_hazard hazard = atEnd ? hazard_assess(here, fromSeconds) : hazard_assess_clearance(here, fromSeconds);
		if (hazard.level == HAZARD_DANGER
			|| hazard_storm_near(here, fromSeconds, untilSeconds, atEnd ? 0U : tuning::kClearanceMm) != nullptr) {
			return marspath::RiskLevel::Danger;
		}
		// Coarse relief alone is information, not a reason to go round
		const bool caution = (hazard.reasons & HAZARD_CAUTION_REASONS & ~HAZARD_COARSE_TERRAIN) != 0;
		return caution ? marspath::RiskLevel::Caution : marspath::RiskLevel::Safe;
	}

	MarspathRoutePlanner::MarspathRoutePlanner()
		: planner_{risk_, terrain_, makeParams(false)}
		, finePlanner_{risk_, terrain_, makeParams(true)}
	{
	}

	Route MarspathRoutePlanner::plan(t_position from, t_position to, std::uint32_t departureSeconds)
	{
		waypoints_.clear();

		// Fetch the weather chunks first: the search never waits for the link
		t_grid grid{};
		if (!grid_init(&grid, mars::toCoord(from), mars::toCoord(to))) {
			RouteSummary summary{};
			summary.status = RouteStatus::TooFar;
			summary.level = HazardLevel::Danger;
			return {waypoints_, summary};
		}
		static_cast<void>(grid_weather_prefetch(&grid));

		risk_.timeSeconds = departureSeconds;
		risk_.start = mars::toCoord(from);
		risk_.goal = mars::toCoord(to);
		marspath::PlanResult result = planner_.plan(toGeoPoint(from), toGeoPoint(to));
		if (result.status == marspath::Status::NoPathFound) {
			result = finePlanner_.plan(toGeoPoint(from), toGeoPoint(to));
		}
		RouteSummary summary = summarise(result, departureSeconds);
		lastStats_ = result.stats;
		lastWarnings_ = result.warnings;
		if (result.ok()) {
			std::vector<t_coord> path;
			path.reserve(result.path.size());
			for (const marspath::Waypoint& waypoint : result.path) {
				path.push_back(toCoord(waypoint.position));
			}
			regularise(path, departureSeconds);
			waypoints_.front() = from;
			waypoints_.back() = to;
			// The summary of what the crew walks: the waypoints
			path.clear();
			for (const t_position& waypoint : waypoints_) {
				path.push_back(mars::toCoord(waypoint));
			}
			summary = summariseCoords(path, departureSeconds);
		}
		return {waypoints_, summary};
	}

	namespace {

		[[nodiscard]] t_coord between(t_coord a, t_coord b, std::uint64_t num, std::uint64_t den) noexcept
		{
			const std::int64_t n = static_cast<std::int64_t>(num);
			const std::int64_t d = static_cast<std::int64_t>(den);
			return {lon_wrap_udeg(std::int64_t{a.lon_udeg} + std::int64_t{lon_delta_udeg(a.lon_udeg, b.lon_udeg)} * n / d),
				static_cast<std::int32_t>(a.lat_udeg + (std::int64_t{b.lat_udeg} - a.lat_udeg) * n / d)};
		}

	}

	bool MarspathRoutePlanner::chordClear(t_coord a, t_coord b, std::uint64_t alongMm, std::uint32_t departureSeconds,
		t_coord start, t_coord goal) const
	{
		const std::uint32_t length = coord_distance_mm(a, b);
		const auto steps = static_cast<std::uint32_t>(length / (tuning::kPathCheckStepM * kMmPerM)) + 1U;
		for (std::uint32_t k = 1; k < steps; ++k) {
			const t_coord c = between(a, b, k, steps);
			const auto at = static_cast<std::uint32_t>(
				std::min<std::uint64_t>(departureSeconds + (alongMm + std::uint64_t{length} * k / steps) / WALK_SPEED_MM_S,
					UINT32_MAX - tuning::kStormMarginSeconds));
			const bool atEnd = coord_distance_mm(start, c) < tuning::kClearanceMm || coord_distance_mm(goal, c) < tuning::kClearanceMm;
			if (risk_.classify(c, at, at + tuning::kStormMarginSeconds, atEnd) == marspath::RiskLevel::Danger) {
				return false;
			}
		}
		return true;
	}

	void MarspathRoutePlanner::regularise(std::span<const t_coord> path, std::uint32_t departureSeconds)
	{
		// A chord that would cut a bend into a danger keeps the path vertex
		waypoints_.clear();
		if (path.empty()) {
			return;
		}
		const auto toPos = [](t_coord c) {
			return t_position{coord_lat_e7(c), static_cast<std::uint32_t>(std::uint64_t{c.lon_udeg} * E7_PER_UDEG)};
		};
		std::vector<std::uint64_t> along(path.size(), 0);
		for (std::size_t i = 1; i < path.size(); ++i) {
			along[i] = along[i - 1] + coord_distance_mm(path[i - 1], path[i]);
		}
		const t_coord start = path.front();
		const t_coord goal = path.back();
		t_coord kept = start;
		std::uint64_t keptAlong = 0;
		std::size_t vertex = 0;  // last path vertex at or before `kept`
		waypoints_.push_back(toPos(start));
		while (keptAlong < along.back()) {
			const std::uint64_t target = std::min<std::uint64_t>(keptAlong + tuning::kWaypointSpacingMm, along.back());
			std::size_t next = vertex;
			while (next + 1 < path.size() && along[next + 1] <= target) {
				++next;
			}
			t_coord candidate = path[next];
			if (next + 1 < path.size() && along[next] < target) {
				candidate = between(path[next], path[next + 1], target - along[next], along[next + 1] - along[next]);
			}
			if (next > vertex && !chordClear(kept, candidate, keptAlong, departureSeconds, start, goal)) {
				candidate = path[vertex + 1];
				next = vertex + 1;
				keptAlong = along[next];
			} else {
				keptAlong = target;
			}
			kept = candidate;
			vertex = next;
			// No duplicate waypoint
			const t_position point = toPos(kept);
			if (point.latitudeE7 != waypoints_.back().latitudeE7
				|| point.longitudeEastE7 != waypoints_.back().longitudeEastE7) {
				waypoints_.push_back(point);
			}
		}
		const t_position end = toPos(goal);
		if (waypoints_.size() == 1) {
			waypoints_.push_back(end);
		} else {
			waypoints_.back() = end;
		}
	}

	RouteSummary MarspathRoutePlanner::assess(std::span<const t_position> waypoints, std::uint32_t departureSeconds)
	{
		RouteSummary failed{};
		failed.status = RouteStatus::NoPath;
		failed.level = HazardLevel::Danger;
		if (waypoints.size() < 2) {
			return failed;
		}
		std::vector<t_coord> coords;
		coords.reserve(waypoints.size());
		for (const t_position& p : waypoints) {
			coords.push_back(mars::toCoord(p));
		}
		// The planner's check, each point at its time of passage
		std::uint64_t along = 0;
		for (std::size_t i = 0; i + 1 < coords.size(); ++i) {
			const std::uint32_t leg = coord_distance_mm(coords[i], coords[i + 1]);
			const auto steps = static_cast<std::uint32_t>(leg / (tuning::kPathCheckStepM * kMmPerM)) + 1U;
			for (std::uint32_t k = 0; k <= steps; ++k) {
				const t_coord c = {lon_wrap_udeg(std::int64_t{coords[i].lon_udeg}
										 + std::int64_t{lon_delta_udeg(coords[i].lon_udeg, coords[i + 1].lon_udeg)} * k / steps),
					static_cast<std::int32_t>(std::int64_t{coords[i].lat_udeg}
						+ (std::int64_t{coords[i + 1].lat_udeg} - coords[i].lat_udeg) * k / steps)};
				const auto at = static_cast<std::uint32_t>(
					std::min<std::uint64_t>(departureSeconds + (along + std::uint64_t{leg} * k / steps) / WALK_SPEED_MM_S, UINT32_MAX));
				const bool atEnd = coord_distance_mm(coords.front(), c) < tuning::kClearanceMm
					|| coord_distance_mm(coords.back(), c) < tuning::kClearanceMm;
				const std::uint32_t until = at > UINT32_MAX - tuning::kStormMarginSeconds ? UINT32_MAX : at + tuning::kStormMarginSeconds;
				const bool danger = risk_.classify(c, at, until, atEnd) == marspath::RiskLevel::Danger;
				if (danger) {
					return failed;
				}
			}
			along += leg;
		}
		return summariseCoords(coords, departureSeconds);
	}

	RouteSummary MarspathRoutePlanner::summariseCoords(std::span<const t_coord> coords, std::uint32_t departureSeconds) const
	{
		RouteSummary summary{};
		summary.status = RouteStatus::Ok;
		const t_route_hazard hazard =
			hazard_route(coords.data(), static_cast<std::uint32_t>(coords.size()), departureSeconds, WALK_SPEED_MM_S);
		// Climb sampled every kPathCheckStepM on the interpolated relief
		std::uint64_t ascentMm = 0;
		std::uint64_t descentMm = 0;
		std::int32_t previous = 0;
		bool known = false;
		const auto climb = [&](t_coord c) {
			std::int32_t mm = 0;
			if (!landform_sample_mm(c, &mm)) {
				return;
			}
			if (known) {
				const std::int64_t dz = std::int64_t{mm} - previous;
				(dz > 0 ? ascentMm : descentMm) += static_cast<std::uint64_t>(dz > 0 ? dz : -dz);
			}
			previous = mm;
			known = true;
		};
		if (!coords.empty()) {
			climb(coords.front());
		}
		for (std::size_t i = 1; i < coords.size(); ++i) {
			const std::uint32_t leg = coord_distance_mm(coords[i - 1], coords[i]);
			const auto steps = static_cast<std::uint32_t>(leg / (tuning::kPathCheckStepM * kMmPerM)) + 1U;
			for (std::uint32_t k = 1; k <= steps; ++k) {
				climb(k == steps ? coords[i] : between(coords[i - 1], coords[i], k, steps));
			}
		}
		const auto ascent = static_cast<std::uint32_t>(std::min<std::uint64_t>((ascentMm + kMmPerM / 2) / kMmPerM, UINT32_MAX));
		const auto descent = static_cast<std::uint32_t>(std::min<std::uint64_t>((descentMm + kMmPerM / 2) / kMmPerM, UINT32_MAX));
		summary.level = toHazardLevel(hazard.level);
		summary.reasons = hazard.reasons;
		summary.lengthM = hazard.length_mm / kMmPerM;
		summary.ascentM = ascent;
		summary.descentM = descent;
		summary.walkSeconds = hazard.duration_s;
		summary.roverSeconds = static_cast<std::uint32_t>(std::uint64_t{summary.lengthM} * kMmPerM / tuning::kRoverSpeedMmPerSecond);
		summary.doseMicroSievert = hazard.dose_eq_usv;
		return summary;
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
