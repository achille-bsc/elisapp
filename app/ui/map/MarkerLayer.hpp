#pragma once

#include <span>
#include <vector>

#include <lvgl.h>

#include "core/map/MapProjection.hpp"
#include "core/poi/PointOfInterest.hpp"

namespace mars::ui {

	/// Told when the user presses a point of interest.
	class IPointOfInterestListener {
	public:
		virtual void onPointOfInterestPressed(const PointOfInterest& point) = 0;

	protected:
		~IPointOfInterestListener() = default;
	};

	/// The dots drawn over the map: the points of interest, the user ("You") and
	/// the destination of the current route.
	class MarkerLayer {
	public:
		/// `points` and its strings must outlive the layer: nothing is copied.
		MarkerLayer(lv_obj_t* map, std::span<const PointOfInterest> points, IPointOfInterestListener& listener);
		MarkerLayer(const MarkerLayer&) = delete;
		MarkerLayer& operator=(const MarkerLayer&) = delete;

		void setCurrentPosition(t_position position) noexcept { current_ = position; }
		void setDestination(t_position position);
		void clearDestination();

		/// Moves every marker to its pixel under `projection`.
		void layout(const MapProjection& projection);

	private:
		static void onMarkerClicked(lv_event_t* event);

		IPointOfInterestListener& listener_;
		std::span<const PointOfInterest> points_;
		std::vector<lv_obj_t*> markers_;
		lv_obj_t* you_;
		lv_obj_t* destination_;
		t_position current_{0, 0};
		t_position destinationPosition_{0, 0};
	};

}
