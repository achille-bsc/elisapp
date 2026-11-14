#include "ui/map/RouteLayer.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kWidth = 4;

	}

	RouteLayer::RouteLayer(lv_obj_t* map)
		: line_{lv_line_create(map)}
	{
		lv_obj_remove_style_all(line_);
		lv_obj_set_clickable(line_, false);
		// Fixed size: the points are then plain map pixels, whatever their extent.
		lv_obj_set_size(line_, lv_pct(100), lv_pct(100));
		lv_obj_set_style_line_color(line_, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
		lv_obj_set_style_line_opa(line_, LV_OPA_80, LV_PART_MAIN);
		lv_obj_set_style_line_width(line_, kWidth, LV_PART_MAIN);
		lv_obj_set_style_line_rounded(line_, true, LV_PART_MAIN);
		widgets::setVisible(line_, false);
	}

	void RouteLayer::setRoute(std::span<const t_position> waypoints)
	{
		waypoints_.assign(waypoints.begin(), waypoints.end());
		widgets::setVisible(line_, waypoints_.size() >= 2);
	}

	void RouteLayer::clear()
	{
		waypoints_.clear();
		pixels_.clear();
		lv_line_set_points(line_, nullptr, 0);
		widgets::setVisible(line_, false);
	}

	void RouteLayer::layout(const MapProjection& projection)
	{
		pixels_.clear();
		pixels_.reserve(waypoints_.size());
		for (const t_position& waypoint : waypoints_) {
			const MapPoint point = projection.toPoint(waypoint);
			pixels_.push_back({point.x, point.y});
		}
		lv_line_set_points(line_, pixels_.data(), static_cast<std::uint32_t>(pixels_.size()));
	}

}
