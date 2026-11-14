#pragma once

#include <span>

#include "core/gps/Position.hpp"

namespace mars {

	/// Computes the path to follow between two positions.
	class IRoutePlanner {
	public:
		IRoutePlanner() = default;
		IRoutePlanner(const IRoutePlanner&) = delete;
		IRoutePlanner& operator=(const IRoutePlanner&) = delete;
		virtual ~IRoutePlanner() = default;

		/// The waypoints from `from` to `to`, both included, or an empty span when
		/// there is no path. Valid until the next call.
		[[nodiscard]] virtual std::span<const t_position> plan(t_position from, t_position to) = 0;
	};

}
