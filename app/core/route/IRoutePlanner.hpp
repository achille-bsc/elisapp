#pragma once

#include <cstdint>

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
	};

}
