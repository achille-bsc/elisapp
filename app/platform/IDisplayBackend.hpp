#pragma once

#include "platform/ScreenGeometry.hpp"

namespace mars::platform {

	/// Opens the LVGL display and its input devices. One implementation per target
	/// (SDL window on a PC, framebuffer or DRM on the device).
	class IDisplayBackend {
	public:
		IDisplayBackend() = default;
		IDisplayBackend(const IDisplayBackend&) = delete;
		IDisplayBackend& operator=(const IDisplayBackend&) = delete;
		virtual ~IDisplayBackend() = default;

		/// Must be called after lv_init(). Returns false if the display cannot be opened.
		[[nodiscard]] virtual bool open(ScreenGeometry geometry, const char* title) = 0;
	};

}
