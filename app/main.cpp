#include <cstdio>
#include <cstdlib>

#include <lvgl.h>

#include "platform/EventLoop.hpp"
#include "platform/ScreenGeometry.hpp"
#include "platform/SdlDisplayBackend.hpp"

int main()
{
	lv_init();
	mars::platform::EventLoop::installTick();

	mars::platform::SdlDisplayBackend display;
	if (!display.open(mars::platform::kIpadMiniLandscape, "Elisapp - Mars GPS")) {
		std::fputs("elisapp: cannot open the display\n", stderr);
		return EXIT_FAILURE;
	}

	mars::platform::EventLoop::run();
}
