#include "ui/map/MarkerLayer.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kDot = 10;
		constexpr std::int32_t kYouDot = 16;
		constexpr std::int32_t kDestinationDot = 22;

		/// Extra pixels around a marker that still count as a press on it.
		constexpr std::int32_t kTouchSlop = (theme::kTouchTarget - kDot) / 2;

		/// A row "dot + text" whose dot is centred on the position of the row.
		lv_obj_t* makeMarker(lv_obj_t* map, std::int32_t dotSize, const char* text, const lv_style_t& textStyle)
		{
			lv_obj_t* const marker = widgets::box(map, &theme::styles().marker);
			lv_obj_set_style_translate_x(marker, -dotSize / 2, LV_PART_MAIN);

			lv_obj_t* const dot = widgets::box(marker, &theme::styles().markerDot);
			lv_obj_set_size(dot, dotSize, dotSize);

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
			lv_obj_t* const marker = makeMarker(map, kDot, point.name, styles.onMapText);
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

		you_ = makeMarker(map, kYouDot, "You", styles.onMapTitle);
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

	void MarkerLayer::layout(const MapProjection& projection)
	{
		for (std::size_t index = 0; index < markers_.size(); ++index) {
			place(markers_[index], projection, points_[index].position);
		}
		place(destination_, projection, destinationPosition_);
		place(you_, projection, current_);
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
