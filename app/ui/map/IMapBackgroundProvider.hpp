#pragma once

#include "core/map/MapProjection.hpp"
#include "ui/map/MapLayer.hpp"

namespace mars::ui {

	/// Supplies the picture drawn under the markers. Implement it on top of the
	/// imagery APIs (terrain mosaic, weather and risk maps).
	class IMapBackgroundProvider {
	public:
		IMapBackgroundProvider() = default;
		IMapBackgroundProvider(const IMapBackgroundProvider&) = delete;
		IMapBackgroundProvider& operator=(const IMapBackgroundProvider&) = delete;
		virtual ~IMapBackgroundProvider() = default;

		/// An LVGL image source (lv_image_dsc_t* or file path) covering exactly the
		/// region shown by `projection`, or nullptr when there is none: the map is
		/// then filled with a plain colour. The source must stay valid until the
		/// next call.
		[[nodiscard]] virtual const void* background(MapLayer layer, const MapProjection& projection) = 0;
	};

}
