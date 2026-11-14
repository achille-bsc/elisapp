#pragma once

#include <cstdint>

namespace mars::platform {

	/// Logical screen size in pixels.
	struct ScreenGeometry {
		std::int32_t width;
		std::int32_t height;
	};

	/// iPad mini (6th generation) in landscape, in points: 2266 x 1488 px at 2x.
	inline constexpr ScreenGeometry kIpadMiniLandscape{1133, 744};

}
