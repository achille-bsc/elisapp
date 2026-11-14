#pragma once

#include <vector>

#include "core/pathfinding/marspath.hpp"
#include "core/route/IRoutePlanner.hpp"

namespace mars {

	/// IRoutePlanner backed by the team's pathfinding (marspath) reading the
	/// relief and the hazards from the data module. A DataSession must be alive
	/// while it is used.
	class MarspathRoutePlanner final : public IRoutePlanner {
	public:
		MarspathRoutePlanner();

		[[nodiscard]] Route plan(t_position from, t_position to, std::uint32_t departureSeconds) override;

	private:
		class Terrain final : public marspath::ITerrain {
		public:
			[[nodiscard]] float elevation(marspath::GeoPoint point) const override;
		};

		class RiskMap final : public marspath::IRiskMap {
		public:
			[[nodiscard]] bool isHazard(marspath::GeoPoint point) const override;

			/// Hazards are assessed at this mission time.
			std::uint32_t timeSeconds{0};
		};

		[[nodiscard]] RouteSummary summarise(const marspath::PlanResult& result, std::uint32_t departureSeconds) const;

		Terrain terrain_;
		RiskMap risk_;
		marspath::Planner planner_;
		std::vector<t_position> waypoints_;
	};

}
