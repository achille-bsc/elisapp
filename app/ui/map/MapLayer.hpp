#pragma once

#include <cstdint>

namespace mars::ui {

	/// The data sets the map can show, one at a time.
	enum class MapLayer : std::uint8_t {
		Terrain,
		Weather,
		Risk,
		Radiation,
	};

	inline constexpr std::uint8_t kMapLayerCount = 4;

}
