#include "ui/map/MapView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		/// Free space kept around the points by fitAll(), in pixels.
		constexpr std::int32_t kFitMargin = 140;

		/// A press that moves less than this is a tap, not a drag (pixels).
		constexpr std::int32_t kDragThreshold = 8;

		/// Each zoom step doubles or halves the scale.
		constexpr double kZoomFactor = 2.0;

		/// Keyboard panning moves the view by this share of its size.
		constexpr std::int32_t kKeyPanDivisor = 4;

		/// While following, the map recentres once the user is this share of
		/// the smaller side away from the centre: not on every fix, since
		/// each recentring computes a new background picture.
		constexpr std::int32_t kFollowDivisor = 6;

		/// Plain fill shown while a layer has no picture, indexed by MapLayer.
		constexpr std::array<std::uint32_t, kMapLayerCount> kFallbackColors{0x8A4B35, 0x2F7F5F, 0x6E5A86};

		lv_obj_t* makeRoot(lv_obj_t* parent)
		{
			lv_obj_t* const root = widgets::box(parent);
			lv_obj_set_size(root, lv_pct(100), lv_pct(100));
			lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
			lv_obj_set_clickable(root, true);
			return root;
		}

	}

	MapView::MapView(lv_obj_t* parent, std::span<const PointOfInterest> points, t_position current,
		IMapBackgroundProvider& backgrounds, IMapListener& mapListener, IPointOfInterestListener& pointListener)
		: backgrounds_{backgrounds}
		, listener_{mapListener}
		, points_{points}
		, root_{makeRoot(parent)}
		, route_{root_}
		, markers_{root_, points, pointListener}
		, current_{current}
	{
		markers_.setCurrentPosition(current_);
		markers_.setDragFlag(&dragging_);
		lv_obj_add_event_cb(root_, onClicked, LV_EVENT_CLICKED, this);
		lv_obj_add_event_cb(root_, onPressed, LV_EVENT_PRESSED, this);
		lv_obj_add_event_cb(root_, onPressing, LV_EVENT_PRESSING, this);
		lv_obj_add_event_cb(root_, onReleased, LV_EVENT_RELEASED, this);
		lv_obj_add_event_cb(root_, onKey, LV_EVENT_KEY, this);
		// The keyboard (arrows, + and -) drives the map when a group is set up.
		if (lv_group_t* const group = lv_group_get_default(); group != nullptr) {
			lv_group_add_obj(group, root_);
		}
		lv_obj_update_layout(root_);
		fitAll();
		// Registered last: the first layout must not ask for a picture of the default region.
		lv_obj_add_event_cb(root_, onSizeChanged, LV_EVENT_SIZE_CHANGED, this);
	}

	void MapView::fitAll()
	{
		std::vector<t_position> positions;
		positions.reserve(points_.size() + 1);
		positions.push_back(current_);
		for (const PointOfInterest& point : points_) {
			positions.push_back(point.position);
		}
		projection_ = MapProjection::fitting(positions, {lv_obj_get_width(root_), lv_obj_get_height(root_)}, kFitMargin);
		layout();
	}

	void MapView::centerOnCurrentPosition()
	{
		projection_.setCenter(current_);
		layout();
	}

	void MapView::fitPositions(std::span<const t_position> positions, std::int32_t bottomInset)
	{
		const MapSize size = projection_.size();
		const std::int32_t inset = std::clamp(bottomInset, 0, size.height / 2);
		const MapProjection fitted = MapProjection::fitting(positions, {size.width, size.height - inset}, kFitMargin);
		// Same scale on the whole view, the fitted area moved up above the inset.
		const MapProjection whole{fitted.center(), fitted.pixelsPerDegree(), size};
		projection_ = MapProjection{whole.toPosition({size.width / 2, size.height / 2 + inset / 2}), fitted.pixelsPerDegree(), size};
		following_ = false;
		layout();
	}

	void MapView::zoomBy(std::int32_t steps)
	{
		const double scale = projection_.pixelsPerDegree() * std::pow(kZoomFactor, steps);
		projection_ = MapProjection{projection_.center(), scale, projection_.size()};
		layout();
	}

	void MapView::panBy(std::int32_t dx, std::int32_t dy)
	{
		projection_.setCenter(centerAfterDrag(-dx, -dy));
		following_ = false;
		listener_.onMapMoved();
		layout();
	}

	void MapView::setFollow(bool follow)
	{
		following_ = follow;
		if (follow) {
			centerOnCurrentPosition();
		}
	}

	void MapView::setCurrentPosition(t_position position)
	{
		current_ = position;
		markers_.setCurrentPosition(position);
		if (following_) {
			const MapPoint point = projection_.toPoint(position);
			const MapSize size = projection_.size();
			const std::int32_t limit = std::min(size.width, size.height) / kFollowDivisor;
			if (std::abs(point.x - size.width / 2) > limit || std::abs(point.y - size.height / 2) > limit) {
				centerOnCurrentPosition();
				return;
			}
		}
		markers_.layout(projection_);
	}

	t_position MapView::centerAfterDrag(std::int32_t dx, std::int32_t dy) const noexcept
	{
		const MapSize size = projection_.size();
		return projection_.toPosition({size.width / 2 - dx, size.height / 2 - dy});
	}

	namespace {

		/// Where the active pointer is, in screen pixels.
		lv_point_t pointerPoint()
		{
			lv_point_t point{0, 0};
			if (lv_indev_t* const input = lv_indev_active(); input != nullptr) {
				lv_indev_get_point(input, &point);
			}
			return point;
		}

	}

	lv_point_t MapView::dragVector() const
	{
		const lv_point_t point = pointerPoint();
		return {point.x - pressedAt_.x, point.y - pressedAt_.y};
	}

	void MapView::setLayer(MapLayer layer)
	{
		layer_ = layer;
		refreshBackground();
	}

	void MapView::setRoute(std::span<const t_position> waypoints)
	{
		route_.setRoute(waypoints);
		route_.layout(projection_);
		if (waypoints.empty()) {
			markers_.clearDestination();
		} else {
			markers_.setDestination(waypoints.back());
		}
		markers_.layout(projection_);
	}

	void MapView::clearRoute()
	{
		route_.clear();
		markers_.clearDestination();
	}

	void MapView::refreshBackground()
	{
		lv_obj_set_style_bg_color(root_, lv_color_hex(kFallbackColors[static_cast<std::size_t>(layer_)]), LV_PART_MAIN);

		const void* const source = backgrounds_.background(layer_, projection_);
		if (source == nullptr) {
			if (image_ != nullptr) {
				lv_image_set_src(image_, nullptr);
				widgets::setVisible(image_, false);
			}
			return;
		}
		if (image_ == nullptr) {
			// Created on demand: no image object as long as no picture is available.
			image_ = lv_image_create(root_);
			lv_obj_remove_style_all(image_);
			lv_obj_set_clickable(image_, false);
			lv_obj_set_size(image_, lv_pct(100), lv_pct(100));
			lv_image_set_inner_align(image_, LV_IMAGE_ALIGN_STRETCH);
			lv_obj_move_to_index(image_, 0);
		}
		lv_image_set_src(image_, source);
		widgets::setVisible(image_, true);
	}

	void MapView::layout()
	{
		route_.layout(projection_);
		markers_.layout(projection_);
		refreshBackground();
	}

	void MapView::onPressed(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
		self->dragging_ = false;
		self->pressedAt_ = pointerPoint();
	}

	void MapView::onPressing(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
		const lv_point_t drag = self->dragVector();
		if (!self->dragging_ && std::abs(drag.x) + std::abs(drag.y) < kDragThreshold) {
			return;
		}
		self->dragging_ = true;
		// Preview: the picture slides, the markers and the route follow it;
		// the background is computed again only on release.
		if (self->image_ != nullptr) {
			lv_obj_set_style_translate_x(self->image_, drag.x, LV_PART_MAIN);
			lv_obj_set_style_translate_y(self->image_, drag.y, LV_PART_MAIN);
		}
		MapProjection preview = self->projection_;
		preview.setCenter(self->centerAfterDrag(drag.x, drag.y));
		self->route_.layout(preview);
		self->markers_.layout(preview);
	}

	void MapView::onReleased(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
		if (!self->dragging_) {
			return;
		}
		const lv_point_t drag = self->dragVector();
		if (self->image_ != nullptr) {
			lv_obj_set_style_translate_x(self->image_, 0, LV_PART_MAIN);
			lv_obj_set_style_translate_y(self->image_, 0, LV_PART_MAIN);
		}
		self->panBy(-drag.x, -drag.y);
	}

	void MapView::onKey(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
		const MapSize size = self->projection_.size();
		const std::int32_t stepX = size.width / kKeyPanDivisor;
		const std::int32_t stepY = size.height / kKeyPanDivisor;
		switch (lv_event_get_key(event)) {
		case LV_KEY_LEFT:
			self->panBy(-stepX, 0);
			break;
		case LV_KEY_RIGHT:
			self->panBy(stepX, 0);
			break;
		case LV_KEY_UP:
			self->panBy(0, -stepY);
			break;
		case LV_KEY_DOWN:
			self->panBy(0, stepY);
			break;
		case '+':
		case '=':
			self->zoomBy(1);
			break;
		case '-':
			self->zoomBy(-1);
			break;
		default:
			break;
		}
	}

	void MapView::onClicked(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
		// The end of a drag is not a tap, nor is a tap on a marker (bubbled up).
		if (self->dragging_ || lv_event_get_target_obj(event) != self->root_) {
			return;
		}
		lv_indev_t* const input = lv_indev_active();
		if (input == nullptr) {
			return;
		}
		lv_point_t pressed;
		lv_indev_get_point(input, &pressed);
		lv_area_t area;
		lv_obj_get_coords(self->root_, &area);
		self->listener_.onMapPressed(self->projection_.toPosition({pressed.x - area.x1, pressed.y - area.y1}));
	}

	void MapView::onSizeChanged(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
		self->projection_.setSize({lv_obj_get_width(self->root_), lv_obj_get_height(self->root_)});
		self->layout();
	}

}
