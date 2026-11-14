#pragma once

#include <span>
#include <vector>

#include <lvgl.h>

#include "core/map/MapProjection.hpp"

namespace mars::ui {

	/// The path drawn over the map from the user to the destination.
	class RouteLayer {
	public:
		explicit RouteLayer(lv_obj_t* map);
		RouteLayer(const RouteLayer&) = delete;
		RouteLayer& operator=(const RouteLayer&) = delete;

		/// Copies `waypoints`, ordered from the start to the destination.
		void setRoute(std::span<const t_position> waypoints);
		void clear();

		/// Redraws the path under `projection`.
		void layout(const MapProjection& projection);

	private:
		lv_obj_t* line_;
		std::vector<t_position> waypoints_;
		std::vector<lv_point_precise_t> pixels_;
	};

}
