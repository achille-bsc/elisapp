#pragma once

#include <cstdint>
#include <span>

#include "core/gps/Position.hpp"
#include "core/route/Route.hpp"

namespace mars {

	/// Computes the path to follow between two positions.
	class IRoutePlanner {
	public:
		IRoutePlanner() = default;
		IRoutePlanner(const IRoutePlanner&) = delete;
		IRoutePlanner& operator=(const IRoutePlanner&) = delete;
		virtual ~IRoutePlanner() = default;

		/// The route from `from` to `to` for a departure at `departureSeconds`
		/// (mission time). Its waypoints are valid until the next call.
		[[nodiscard]] virtual Route plan(t_position from, t_position to, std::uint32_t departureSeconds) = 0;

		/// What following `waypoints` (start first) from `departureSeconds`
		/// costs and risks, with the same checks as a planned route: status
		/// Ok only if it keeps clear of every danger. Planners that cannot
		/// check a given path keep this default (status Failed).
		[[nodiscard]] virtual RouteSummary assess(std::span<const t_position> waypoints, std::uint32_t departureSeconds)
		{
			static_cast<void>(waypoints);
			static_cast<void>(departureSeconds);
			RouteSummary summary{};
			summary.status = RouteStatus::Failed;
			summary.level = HazardLevel::Danger;
			return summary;
		}
	};

}
