#pragma once

#include <array>
#include <cstdint>

#include <lvgl.h>

#include "ui/map/MapLayer.hpp"

namespace mars::ui {

	/// Told when the user picks a map layer.
	class ILayerListener {
	public:
		virtual void onLayerSelected(MapLayer layer) = 0;

	protected:
		~ILayerListener() = default;
	};

	/// The pill opened by the layers button: one segment per map layer.
	class LayerSwitcher {
	public:
		LayerSwitcher(lv_obj_t* parent, ILayerListener& listener);
		LayerSwitcher(const LayerSwitcher&) = delete;
		LayerSwitcher& operator=(const LayerSwitcher&) = delete;

		void setVisible(bool visible);

		/// Shifts the pill `offset` pixels to the right of its place in the layout.
		void setOffset(std::int32_t offset);
		[[nodiscard]] bool isVisible() const;

		/// Highlights the segment of `layer`.
		void setSelected(MapLayer layer);

	private:
		static void onSegment(lv_event_t* event);

		ILayerListener& listener_;
		lv_obj_t* root_;
		std::array<lv_obj_t*, kMapLayerCount> segments_{};
	};

}
