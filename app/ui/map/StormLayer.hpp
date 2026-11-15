#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <lvgl.h>

#include "core/map/MapProjection.hpp"
#include "core/weather/StormForecast.hpp"

namespace mars::ui {

	/// Forecast storms over every layer, with their name and time window.
	class StormLayer {
	public:
		static constexpr std::size_t kMaxStorms = 8;

		explicit StormLayer(lv_obj_t* map);
		StormLayer(const StormLayer&) = delete;
		StormLayer& operator=(const StormLayer&) = delete;

		/// Names must outlive the layer.
		void setStorms(std::span<const StormView> storms);
		void layout(const MapProjection& projection);

	private:
		std::array<lv_obj_t*, kMaxStorms> discs_{};
		std::array<lv_obj_t*, kMaxStorms> labels_{};
		std::array<std::array<char, 80>, kMaxStorms> texts_{};
		std::array<StormView, kMaxStorms> storms_{};
		std::size_t count_{0};
	};

}
