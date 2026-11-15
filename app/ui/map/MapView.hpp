#pragma once

#include <span>

#include <lvgl.h>

#include "core/map/MapProjection.hpp"
#include "ui/map/IMapBackgroundProvider.hpp"
#include "ui/map/MapLayer.hpp"
#include "ui/map/MarkerLayer.hpp"
#include "ui/map/RouteLayer.hpp"

namespace mars::ui {

	/// Told when the user presses the map itself, away from any marker.
	class IMapListener {
	public:
		virtual void onMapPressed(t_position position) = 0;

		/// The user moved the map away (drag, keys): it stops following them.
		virtual void onMapMoved() {}

	protected:
		~IMapListener() = default;
	};

	/// The full-screen map: a background picture supplied by an
	/// IMapBackgroundProvider, with the route and the markers over it.
	class MapView {
	public:
		/// `points`, `backgrounds` and both listeners must outlive the view, and
		/// the view must live as long as `parent`: its widgets call back into it.
		/// The map starts fitted around `points` and `current`, the user's position.
		MapView(lv_obj_t* parent, std::span<const PointOfInterest> points, t_position current,
			IMapBackgroundProvider& backgrounds, IMapListener& mapListener, IPointOfInterestListener& pointListener);
		MapView(const MapView&) = delete;
		MapView& operator=(const MapView&) = delete;

		/// Shows every point of interest and the current position at once.
		void fitAll();

		/// Centres the map on the current position, keeping the scale.
		void centerOnCurrentPosition();

		/// Shows all of `positions` (a route) in the part of the map above
		/// `bottomInset` pixels, which a sheet covers.
		void fitPositions(std::span<const t_position> positions, std::int32_t bottomInset);

		/// Zooms in (steps > 0) or out by factors of two, around the centre.
		void zoomBy(std::int32_t steps);

		/// Moves the view by a number of pixels (positive: east and south).
		void panBy(std::int32_t dx, std::int32_t dy);

		/// While following, the map keeps the current position near its centre.
		void setFollow(bool follow);
		[[nodiscard]] bool isFollowing() const noexcept { return following_; }

		void setCurrentPosition(t_position position);
		void setLayer(MapLayer layer);
		[[nodiscard]] MapLayer layer() const noexcept { return layer_; }

		/// Draws `waypoints` (start first) and marks the last one as the destination.
		void setRoute(std::span<const t_position> waypoints);
		void clearRoute();

		/// Asks the provider again for the picture of the current layer and
		/// region. Call it when new imagery has been received.
		void refreshBackground();

	private:
		static void onClicked(lv_event_t* event);
		static void onSizeChanged(lv_event_t* event);
		static void onPressed(lv_event_t* event);
		static void onPressing(lv_event_t* event);
		static void onReleased(lv_event_t* event);
		static void onKey(lv_event_t* event);

		void layout();
		/// The position under the pixel that a drag of (dx, dy) brings to the centre.
		[[nodiscard]] t_position centerAfterDrag(std::int32_t dx, std::int32_t dy) const noexcept;
		[[nodiscard]] lv_point_t dragVector() const;

		IMapBackgroundProvider& backgrounds_;
		IMapListener& listener_;
		std::span<const PointOfInterest> points_;
		lv_obj_t* root_;
		lv_obj_t* image_{nullptr};
		RouteLayer route_;
		MarkerLayer markers_;
		MapProjection projection_;
		t_position current_;
		MapLayer layer_{MapLayer::Terrain};
		lv_point_t pressedAt_{0, 0};
		bool dragging_{false};
		bool following_{false};
	};

}
