#include "ui/map/WaypointLayer.hpp"

#include <algorithm>
#include <cstdio>

#include "data/core/geodesy.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kSize = 26;
		constexpr std::int32_t kBorder = 2;
		/// Smallest distance between two shown markers (pixels).
		constexpr std::int32_t kSpacing = kSize + 8;
		constexpr std::int32_t kTouchSlop = (theme::kTouchTarget - kSize) / 2;
		/// Okabe-Ito safe, caution, danger.
		constexpr std::uint32_t kLevelColors[] = {0x009E73, 0xF0E442, 0xD55E00};
		constexpr std::uint32_t kNumberColors[] = {0xFFFFFF, 0x000000, 0xFFFFFF};

	}

	std::size_t spreadAlongRoute(std::span<const t_position> route, std::uint32_t count, std::span<RouteMark> out)
	{
		if (route.size() < 2 || count == 0 || out.empty()) {
			return 0;
		}
		const auto legMm = [&route](std::size_t i) -> std::uint64_t {
			const t_position a = route[i - 1];
			const t_position b = route[i];
			return coord_distance_mm(coord_from_e7_east(a.latitudeE7, a.longitudeEastE7),
				coord_from_e7_east(b.latitudeE7, b.longitudeEastE7));
		};
		std::uint64_t total = 0;
		for (std::size_t i = 1; i < route.size(); ++i) {
			total += legMm(i);
		}
		const std::uint64_t spacing = std::max<std::uint64_t>(1, total / count);
		std::size_t written = 0;
		std::uint64_t along = 0;
		std::uint64_t next = spacing;
		for (std::size_t i = 1; i < route.size(); ++i) {
			along += legMm(i);
			if (i + 1 == route.size()) {
				out[written++] = {i, along};  // a slot is always kept for it
				break;
			}
			// Keep walking: not at the next mark, too close to the end, or only
			// the destination's slot left
			if (along < next || along + spacing / 2 > total || written + 1 >= out.size()) {
				continue;
			}
			out[written++] = {i, along};
			while (next <= along) {
				next += spacing;
			}
		}
		return written;
	}

	WaypointLayer::WaypointLayer(lv_obj_t* map, INavigationPointListener& listener)
		: listener_{listener}
	{
		for (std::size_t i = 0; i < kMaxPoints; ++i) {
			lv_obj_t* const marker = widgets::box(map);
			lv_obj_set_size(marker, kSize, kSize);
			lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
			lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, LV_PART_MAIN);
			lv_obj_set_style_border_width(marker, kBorder, LV_PART_MAIN);
			lv_obj_set_style_border_color(marker, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
			lv_obj_set_style_translate_x(marker, -kSize / 2, LV_PART_MAIN);
			lv_obj_set_style_translate_y(marker, -kSize / 2, LV_PART_MAIN);
			lv_obj_set_clickable(marker, true);
			lv_obj_set_ext_click_area(marker, kTouchSlop);
			lv_obj_add_event_cb(marker, onClicked, LV_EVENT_CLICKED, this);
			lv_obj_set_event_bubble(marker, true);
			labels_[i] = widgets::label(marker, "");
			lv_obj_add_style(labels_[i], &theme::styles().smallText, LV_PART_MAIN);
			lv_obj_center(labels_[i]);
			widgets::setVisible(marker, false);
			markers_[i] = marker;
		}
	}

	void WaypointLayer::setPoints(std::span<const NavigationPoint> points)
	{
		points_.assign(points.begin(), points.begin() + static_cast<std::ptrdiff_t>(std::min(points.size(), kMaxPoints)));
		for (std::size_t i = 0; i < kMaxPoints; ++i) {
			const bool used = i < points_.size();
			widgets::setVisible(markers_[i], used);
			if (!used) {
				continue;
			}
			const auto level = static_cast<std::size_t>(points_[i].level);
			lv_obj_set_style_bg_color(markers_[i], lv_color_hex(kLevelColors[level]), LV_PART_MAIN);
			lv_obj_set_style_border_color(markers_[i], lv_color_hex(points_[i].back ? theme::kRouteBack : theme::kOnMap),
				LV_PART_MAIN);
			lv_obj_set_style_text_color(labels_[i], lv_color_hex(kNumberColors[level]), LV_PART_MAIN);
			std::snprintf(numbers_[i].data(), numbers_[i].size(), "%u", static_cast<unsigned>(points_[i].number));
			lv_label_set_text_static(labels_[i], numbers_[i].data());
			lv_obj_set_user_data(markers_[i], reinterpret_cast<void*>(i));
		}
		// Leg ends drawn on top so they stay visible and tappable
		for (std::size_t i = 0; i < points_.size(); ++i) {
			if (i + 1 == points_.size() || points_[i + 1].back != points_[i].back) {
				lv_obj_move_foreground(markers_[i]);
			}
		}
	}

	void WaypointLayer::clear()
	{
		setPoints({});
	}

	void WaypointLayer::layout(const MapProjection& projection)
	{
		// Overlapping markers are hidden; leg ends are placed first
		std::array<MapPoint, kMaxPoints> shown{};
		std::size_t count = 0;
		const auto legEnd = [this](std::size_t i) {
			return i + 1 == points_.size() || points_[i + 1].back != points_[i].back;
		};
		for (const bool ends : {true, false}) {
			for (std::size_t i = 0; i < points_.size(); ++i) {
				if (legEnd(i) != ends) {
					continue;
				}
				const MapPoint point = projection.toPoint(points_[i].position);
				lv_obj_set_pos(markers_[i], point.x, point.y);
				bool free = true;
				for (std::size_t k = 0; k < count; ++k) {
					const std::int32_t dx = point.x - shown[k].x;
					const std::int32_t dy = point.y - shown[k].y;
					free = free && dx * dx + dy * dy >= kSpacing * kSpacing;
				}
				widgets::setVisible(markers_[i], free);
				if (free) {
					shown[count++] = point;
				}
			}
		}
	}

	void WaypointLayer::onClicked(lv_event_t* event)
	{
		auto* const self = static_cast<WaypointLayer*>(lv_event_get_user_data(event));
		if (self->dragging_ != nullptr && *self->dragging_) {
			return;
		}
		const auto index = reinterpret_cast<std::size_t>(lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
		if (index < self->points_.size()) {
			self->listener_.onNavigationPointPressed(self->points_[index]);
		}
	}

	static_assert(std::size(kLevelColors) == 3 && std::size(kNumberColors) == 3, "one per HazardLevel");

}
