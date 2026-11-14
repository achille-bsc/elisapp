#pragma once

#include <array>

#include "core/route/IRoutePlanner.hpp"

namespace mars {

	/// Placeholder planner: a straight line. To be replaced by an adapter around
	/// marspath::Planner once the terrain and risk maps are available.
	class DirectRoutePlanner final : public IRoutePlanner {
	public:
		[[nodiscard]] std::span<const t_position> plan(t_position from, t_position to) override
		{
			waypoints_ = {from, to};
			return waypoints_;
		}

	private:
		std::array<t_position, 2> waypoints_{};
	};

}
