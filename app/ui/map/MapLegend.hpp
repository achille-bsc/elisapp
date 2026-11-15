#pragma once

#include <array>

#include <lvgl.h>

#include "ui/map/render/Legend.hpp"

namespace mars::ui {

	/// Legend card of the current layer, with a scale bar (north is up).
	class MapLegend {
	public:
		explicit MapLegend(lv_obj_t* parent);
		MapLegend(const MapLegend&) = delete;
		MapLegend& operator=(const MapLegend&) = delete;

		void show(const LegendSpec& spec, double metresPerPixel);
		void setVisible(bool visible);
		/// Back over the map widgets created or raised after it.
		void raise() { lv_obj_move_foreground(root_); }

	private:
		struct Row {
			lv_obj_t* root;
			lv_obj_t* swatch;
			lv_obj_t* hatch;
			lv_obj_t* label;
		};

		void showScale(double metresPerPixel);

		lv_obj_t* root_;
		lv_obj_t* title_;
		std::array<Row, kLegendMaxItems> rows_{};
		lv_obj_t* gradient_;
		std::array<lv_obj_t*, kLegendGradientStops - 1> gradientParts_{};
		lv_obj_t* range_;
		lv_obj_t* low_;
		lv_obj_t* high_;
		lv_obj_t* note_;
		lv_obj_t* scaleBar_;
		lv_obj_t* scaleLabel_;
		LegendSpec spec_{};
		std::array<char, 24> scaleText_{};
	};

}
