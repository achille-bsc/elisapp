#include "ui/map/MarkerLayer.hpp"

#include <algorithm>

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kDot = 10;
		constexpr std::int32_t kYouDot = 20;
		constexpr std::int32_t kYouRing = 3;
		constexpr std::int32_t kYouHalo = 56;
		constexpr std::int32_t kConeSize = 96;
		constexpr std::int32_t kConeHalfAngle = 28;
		constexpr std::int32_t kDegPerTurn = 360;
		/// lv_arc measures from the east (3 o'clock); north is 270 there.
		constexpr std::int32_t kArcNorth = 270;
		constexpr std::int32_t kDestinationDot = 22;

		/// Extra pixels around a marker that still count as a press on it.
		constexpr std::int32_t kTouchSlop = (theme::kTouchTarget - kDot) / 2;

		const char* iconOf(PlaceCategory category)
		{
			switch (category) {
			case PlaceCategory::Hardware:
				return LV_SYMBOL_SETTINGS;
			case PlaceCategory::Site:
				return LV_SYMBOL_HOME;
			default:
				return nullptr;
			}
		}

		constexpr std::int32_t kIconDot = 24;
		constexpr std::uint32_t kIconDotColor = theme::kPanel;
		/// Labels closer than this to another one are hidden (pixels).
		constexpr std::int32_t kLabelGap = 4;

		/// A row "dot + text" whose dot is centred on the position of the row.
		lv_obj_t* makeMarker(lv_obj_t* map, std::int32_t dotSize, const char* text, const lv_style_t& textStyle,
			const char* icon = nullptr)
		{
			lv_obj_t* const marker = widgets::box(map, &theme::styles().marker);
			lv_obj_set_style_translate_x(marker, -dotSize / 2, LV_PART_MAIN);

			lv_obj_t* const dot = widgets::box(marker, &theme::styles().markerDot);
			lv_obj_set_size(dot, dotSize, dotSize);
			if (icon != nullptr) {
				lv_obj_set_style_bg_color(dot, lv_color_hex(kIconDotColor), LV_PART_MAIN);
				lv_obj_set_style_border_color(dot, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
				lv_obj_set_style_border_width(dot, 2, LV_PART_MAIN);
				lv_obj_t* const symbol = widgets::label(dot, icon);
				lv_obj_add_style(symbol, &theme::styles().onMapText, LV_PART_MAIN);
				lv_obj_center(symbol);
			}

			if (text != nullptr) {
				lv_obj_add_style(widgets::label(marker, text), &textStyle, LV_PART_MAIN);
			}
			return marker;
		}

		void place(lv_obj_t* marker, const MapProjection& projection, t_position position)
		{
			const MapPoint point = projection.toPoint(position);
			lv_obj_set_pos(marker, point.x, point.y);
		}

	}

	MarkerLayer::MarkerLayer(lv_obj_t* map, std::span<const PointOfInterest> points, IPointOfInterestListener& listener)
		: listener_{listener}
		, points_{points}
	{
		const theme::Styles& styles = theme::styles();

		markers_.reserve(points_.size());
		for (const PointOfInterest& point : points_) {
			const char* const icon = iconOf(point.category);
			lv_obj_t* const marker = makeMarker(map, icon != nullptr ? kIconDot : kDot, point.name, styles.onMapText, icon);
			lv_obj_set_clickable(marker, true);
			lv_obj_set_ext_click_area(marker, kTouchSlop);
			// LVGL keeps user data as void*; the point is only ever read back as const.
			lv_obj_set_user_data(marker, const_cast<PointOfInterest*>(&point));
			lv_obj_add_event_cb(marker, onMarkerClicked, LV_EVENT_CLICKED, this);
			// Presses reach the map too: a drag started on a marker moves the map.
			lv_obj_set_event_bubble(marker, true);
			markers_.push_back(marker);
		}

		destination_ = makeMarker(map, kDestinationDot, nullptr, styles.onMapText);
		widgets::setVisible(destination_, false);

		// Blue dot, white ring, halo and a heading cone
		you_ = widgets::box(map);
		lv_obj_set_size(you_, kConeSize, kConeSize);
		lv_obj_set_style_translate_x(you_, -kConeSize / 2, LV_PART_MAIN);
		lv_obj_set_style_translate_y(you_, -kConeSize / 2, LV_PART_MAIN);
		lv_obj_t* const halo = widgets::box(you_);
		lv_obj_set_size(halo, kYouHalo, kYouHalo);
		lv_obj_center(halo);
		lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_bg_color(halo, lv_color_hex(theme::kPosition), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(halo, LV_OPA_20, LV_PART_MAIN);
		cone_ = lv_arc_create(you_);
		lv_obj_set_size(cone_, kConeSize, kConeSize);
		lv_obj_center(cone_);
		lv_obj_set_clickable(cone_, false);
		lv_obj_remove_style(cone_, nullptr, LV_PART_KNOB);
		lv_obj_set_style_arc_opa(cone_, LV_OPA_TRANSP, LV_PART_MAIN);
		// As wide as the radius: a wedge
		lv_obj_set_style_arc_width(cone_, kConeSize / 2, LV_PART_INDICATOR);
		lv_obj_set_style_arc_rounded(cone_, false, LV_PART_INDICATOR);
		lv_obj_set_style_arc_color(cone_, lv_color_hex(theme::kPosition), LV_PART_INDICATOR);
		lv_obj_set_style_arc_opa(cone_, LV_OPA_40, LV_PART_INDICATOR);
		widgets::setVisible(cone_, false);
		lv_obj_t* const dot = widgets::box(you_);
		lv_obj_set_size(dot, kYouDot, kYouDot);
		lv_obj_center(dot);
		lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_bg_color(dot, lv_color_hex(theme::kPosition), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_set_style_border_color(dot, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
		lv_obj_set_style_border_width(dot, kYouRing, LV_PART_MAIN);
		lv_obj_set_style_shadow_width(dot, kYouRing * 3, LV_PART_MAIN);
		lv_obj_set_style_shadow_opa(dot, LV_OPA_40, LV_PART_MAIN);
	}

	void MarkerLayer::setDestination(t_position position)
	{
		destinationPosition_ = position;
		widgets::setVisible(destination_, true);
	}

	void MarkerLayer::clearDestination()
	{
		widgets::setVisible(destination_, false);
	}

	void MarkerLayer::setHeading(bool known, std::uint16_t degrees)
	{
		widgets::setVisible(cone_, known);
		if (!known) {
			return;
		}
		const std::int32_t start = (kArcNorth + degrees - kConeHalfAngle + kDegPerTurn) % kDegPerTurn;
		lv_arc_set_bg_angles(cone_, 0, kDegPerTurn);
		lv_arc_set_rotation(cone_, start);
		lv_arc_set_angles(cone_, 0, 2 * kConeHalfAngle);
	}

	void MarkerLayer::layout(const MapProjection& projection)
	{
		for (std::size_t index = 0; index < markers_.size(); ++index) {
			place(markers_[index], projection, points_[index].position);
		}
		place(destination_, projection, destinationPosition_);
		place(you_, projection, current_);
		declutter(projection);
	}

	void MarkerLayer::declutter(const MapProjection& projection)
	{
		// Overlapping labels are hidden (equipment and sites first); dots stay
		std::vector<lv_area_t> taken;
		taken.reserve(markers_.size() + 1);
		const MapPoint you = projection.toPoint(current_);
		const std::int32_t halo = theme::kTouchTarget / 2;
		taken.push_back({you.x - halo, you.y - halo, you.x + halo, you.y + halo});
		for (const bool important : {true, false}) {
			for (std::size_t index = 0; index < markers_.size(); ++index) {
				if ((points_[index].category != PlaceCategory::Feature) != important) {
					continue;
				}
				lv_obj_t* const label = lv_obj_get_child(markers_[index], 1);
				if (label == nullptr) {
					continue;
				}
				const MapPoint at = projection.toPoint(points_[index].position);
				// Measure dot + gap + label: the row shrinks when the label hides
				const std::int32_t w = lv_obj_get_width(lv_obj_get_child(markers_[index], 0)) + theme::kMarkerLabelGap
					+ lv_obj_get_width(label);
				const std::int32_t h = std::max(lv_obj_get_height(label), lv_obj_get_height(lv_obj_get_child(markers_[index], 0)));
				const lv_area_t box{at.x, at.y - h / 2, at.x + w, at.y + h / 2};
				bool free = true;
				for (const lv_area_t& other : taken) {
					free = free && (box.x2 + kLabelGap < other.x1 || other.x2 + kLabelGap < box.x1
						|| box.y2 + kLabelGap < other.y1 || other.y2 + kLabelGap < box.y1);
				}
				widgets::setVisible(label, free);
				if (free) {
					taken.push_back(box);
				}
			}
		}
	}

	void MarkerLayer::onMarkerClicked(lv_event_t* event)
	{
		auto* const self = static_cast<MarkerLayer*>(lv_event_get_user_data(event));
		if (self->dragging_ != nullptr && *self->dragging_) {
			return;
		}
		const auto* const point = static_cast<const PointOfInterest*>(lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
		self->listener_.onPointOfInterestPressed(*point);
	}

}
