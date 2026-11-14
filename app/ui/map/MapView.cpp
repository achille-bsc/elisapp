#include "ui/map/MapView.hpp"

#include <array>
#include <vector>

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		/// Free space kept around the points by fitAll(), in pixels.
		constexpr std::int32_t kFitMargin = 140;

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
		lv_obj_add_event_cb(root_, onClicked, LV_EVENT_CLICKED, this);
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

	void MapView::setCurrentPosition(t_position position)
	{
		current_ = position;
		markers_.setCurrentPosition(position);
		markers_.layout(projection_);
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

	void MapView::onClicked(lv_event_t* event)
	{
		auto* const self = static_cast<MapView*>(lv_event_get_user_data(event));
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
