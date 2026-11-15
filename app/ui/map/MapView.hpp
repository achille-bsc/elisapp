#pragma once

#include <span>

#include <lvgl.h>

#include "core/map/MapProjection.hpp"
#include "ui/map/IMapBackgroundProvider.hpp"
#include "ui/map/MapLayer.hpp"
#include "ui/map/MapLegend.hpp"
#include "ui/map/MarkerLayer.hpp"
#include "ui/map/RouteLayer.hpp"
#include "ui/map/StormLayer.hpp"
#include "ui/map/WaypointLayer.hpp"
#include "ui/map/render/Legend.hpp"

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

	/// Everything a MapView calls back. All of it must outlive the view.
	struct MapViewListeners {
		IMapListener& map;
		IPointOfInterestListener& points;
		INavigationPointListener& navigation;
	};

	/// Full-screen map: background picture, routes, points, markers, legend.
	class MapView {
	public:
		/// Everything given must outlive the view; the view lives as `parent`.
		MapView(lv_obj_t* parent, std::span<const PointOfInterest> points, t_position current,
			IMapBackgroundProvider& backgrounds, const IMapLegendSource& legends, const MapViewListeners& listeners);
		MapView(const MapView&) = delete;
		MapView& operator=(const MapView&) = delete;

		/// Shows every point of interest and the current position at once.
		void fitAll();

		/// Centres the map on the current position, keeping the scale.
		void centerOnCurrentPosition();

		/// `radiusM` of ground from the centre to the nearest edge.
		void showAround(t_position center, std::uint32_t radiusM);

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

		void setHeading(bool known, std::uint16_t degrees) { markers_.setHeading(known, degrees); }
		void setLayer(MapLayer layer);
		[[nodiscard]] MapLayer layer() const noexcept { return layer_; }

		/// Draws `waypoints` (start first) and marks the last one as the destination.
		void setRoute(std::span<const t_position> waypoints);

		/// The way back (dashed); empty: none.
		void setReturnRoute(std::span<const t_position> waypoints);
		void clearRoute();

		void setNavigationPoints(std::span<const NavigationPoint> points);

		void setLegendVisible(bool visible);

		void setStorms(std::span<const StormView> storms);

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
		const IMapLegendSource& legends_;
		IMapListener& listener_;
		std::span<const PointOfInterest> points_;
		lv_obj_t* root_;
		lv_obj_t* image_{nullptr};
		StormLayer storms_;
		RouteLayer route_;
		RouteLayer return_;
		WaypointLayer waypoints_;
		MarkerLayer markers_;
		MapLegend legend_;
		MapProjection projection_;
		t_position current_;
		MapLayer layer_{MapLayer::Terrain};
		lv_point_t pressedAt_{0, 0};
		bool dragging_{false};
		bool following_{false};
	};

}
