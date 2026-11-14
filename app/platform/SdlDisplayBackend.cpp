#include "platform/SdlDisplayBackend.hpp"

#include <lvgl.h>

namespace mars::platform {

	bool SdlDisplayBackend::open(ScreenGeometry geometry, const char* title)
	{
		lv_display_t* const display = lv_sdl_window_create(geometry.width, geometry.height);
		if (display == nullptr) {
			return false;
		}
		lv_sdl_window_set_title(display, title);
		return lv_sdl_mouse_create() != nullptr && lv_sdl_keyboard_create() != nullptr;
	}

}
