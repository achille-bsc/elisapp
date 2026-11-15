#pragma once

#include <lvgl.h>

#include "core/poi/PointOfInterest.hpp"
#include "ui/PositionText.hpp"

namespace mars::ui {

	/// Told when the user leaves the card.
	class IPointOfInterestCardListener {
	public:
		virtual void onPointOfInterestClosed() = 0;

	protected:
		~IPointOfInterestCardListener() = default;
	};

	/// Supplies the photo of a place as an LVGL image source.
	class IPlacePhotos {
	public:
		/// The photo of `point`, valid while the supplier lives; nullptr if none.
		[[nodiscard]] virtual const void* photo(const PointOfInterest& point) = 0;

	protected:
		~IPlacePhotos() = default;
	};

	/// The card describing a point of interest: picture and its credit, name,
	/// description and coordinates.
	class PointOfInterestCard {
	public:
		PointOfInterestCard(lv_obj_t* parent, IPointOfInterestCardListener& listener);
		PointOfInterestCard(const PointOfInterestCard&) = delete;
		PointOfInterestCard& operator=(const PointOfInterestCard&) = delete;

		/// Shows `point`, whose strings must stay valid while the card is visible.
		/// `picture` is an LVGL image source (lv_image_dsc_t* or file path) that
		/// must stay valid too, or nullptr for the empty frame.
		void show(const PointOfInterest& point, const void* picture = nullptr);
		void hide();
		[[nodiscard]] bool isVisible() const;

	private:
		static void onBack(lv_event_t* event);

		IPointOfInterestCardListener& listener_;
		lv_obj_t* root_;
		lv_obj_t* frame_;
		lv_obj_t* picture_{nullptr};
		lv_obj_t* credit_{nullptr};
		lv_obj_t* name_;
		lv_obj_t* description_;
		lv_obj_t* coordinates_;
		PositionText coordinatesText_{};
	};

}
