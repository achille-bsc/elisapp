#pragma once

#include <vector>

#include "core/pathfinding/marspath.hpp"
#include "core/route/IRoutePlanner.hpp"
#include "data/core/coord.hpp"

namespace mars {

	/// IRoutePlanner on marspath and the data module (DataSession alive).
	class MarspathRoutePlanner final : public IRoutePlanner {
	public:
		MarspathRoutePlanner();

		[[nodiscard]] Route plan(t_position from, t_position to, std::uint32_t departureSeconds) override;
		[[nodiscard]] RouteSummary assess(std::span<const t_position> waypoints, std::uint32_t departureSeconds) override;

		[[nodiscard]] const marspath::PlanStatistics& lastStatistics() const noexcept { return lastStats_; }
		[[nodiscard]] marspath::Warning lastWarnings() const noexcept { return lastWarnings_; }

	private:
		class Terrain final : public marspath::ITerrain {
		public:
			[[nodiscard]] float elevation(marspath::GeoPoint point) const override;
		};

		class RiskMap final : public marspath::IRiskMap {
		public:
			[[nodiscard]] bool isHazard(marspath::GeoPoint point) const override;
			[[nodiscard]] marspath::RiskLevel level(marspath::GeoPoint point) const override;
			/// `atEnd`: next to the start or the goal (no 50 m clearance).
			[[nodiscard]] marspath::RiskLevel classify(::t_coord here, std::uint32_t fromSeconds, std::uint32_t untilSeconds,
				bool atEnd) const;

			/// Hazards are assessed at this mission time.
			std::uint32_t timeSeconds{0};
			::t_coord start{};
			::t_coord goal{};
		};

		[[nodiscard]] RouteSummary summariseCoords(std::span<const t_coord> coords, std::uint32_t departureSeconds) const;
		[[nodiscard]] RouteSummary summarise(const marspath::PlanResult& result, std::uint32_t departureSeconds) const;

		Terrain terrain_;
		RiskMap risk_;
		marspath::Planner planner_;
		/// Finer grid, tried when planner_ finds no way.
		marspath::Planner finePlanner_;
		/// Waypoints about every kWaypointSpacingMm, every chord clear.
		void regularise(std::span<const ::t_coord> path, std::uint32_t departureSeconds);
		[[nodiscard]] bool chordClear(::t_coord a, ::t_coord b, std::uint64_t alongMm, std::uint32_t departureSeconds,
			::t_coord start, ::t_coord goal) const;

		std::vector<t_position> waypoints_;
		marspath::PlanStatistics lastStats_{};
		marspath::Warning lastWarnings_{marspath::Warning::None};
	};

}
