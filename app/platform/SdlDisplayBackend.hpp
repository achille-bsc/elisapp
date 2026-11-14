#pragma once

#include "platform/IDisplayBackend.hpp"

namespace mars::platform {

	/// Desktop simulator: SDL2 window with mouse and keyboard input.
	class SdlDisplayBackend final : public IDisplayBackend {
	public:
		[[nodiscard]] bool open(ScreenGeometry geometry, const char* title) override;
	};

}
