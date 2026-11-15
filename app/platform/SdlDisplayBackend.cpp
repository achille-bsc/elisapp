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
		lv_indev_t* const keyboard = lv_sdl_keyboard_create();
		if (lv_sdl_mouse_create() == nullptr || keyboard == nullptr) {
			return false;
		}
		// Keys go to the default group: the map adds itself to it (arrows, + and -).
		lv_group_t* const group = lv_group_create();
		lv_group_set_default(group);
		lv_indev_set_group(keyboard, group);
		return true;
	}

}
