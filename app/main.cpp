#include <cstdio>
#include <cstdlib>

#include <lvgl.h>

#include "core/mission/PlaceholderMissionRepository.hpp"
#include "core/poi/JsonPointOfInterestRepository.hpp"
#include "core/route/DirectRoutePlanner.hpp"
#include "platform/EventLoop.hpp"
#include "platform/ScreenGeometry.hpp"
#include "platform/SdlDisplayBackend.hpp"
#include "ui/MainScreen.hpp"
#include "ui/map/NoMapBackgroundProvider.hpp"

namespace {

	constexpr const char* kPointsOfInterestPath = "assets/points_of_interest.json";

	/// Placeholder position, until the GPS packets are received: Jezero crater rim.
	constexpr mars::t_position kStartPosition{150'000'000, 745'000'000};

	/// Placeholder for the weather screen, which is not implemented yet.
	class WeatherReportStub final : public mars::ui::IMainScreenListener {
	public:
		void onWeatherReportRequested(const mars::Mission& mission) override
		{
			std::printf("elisapp: weather report requested for \"%s\"\n", mission.name);
		}
	};

}

int main()
{
	lv_init();
	mars::platform::EventLoop::installTick();

	mars::platform::SdlDisplayBackend display;
	if (!display.open(mars::platform::kIpadMiniLandscape, "Elisapp - Mars GPS")) {
		std::fputs("elisapp: cannot open the display\n", stderr);
		return EXIT_FAILURE;
	}

	mars::JsonPointOfInterestRepository points;
	if (!points.load(kPointsOfInterestPath)) {
		std::fprintf(stderr, "elisapp: cannot read %s, no point of interest shown\n", kPointsOfInterestPath);
	}
	mars::PlaceholderMissionRepository missions;
	mars::DirectRoutePlanner routes;
	mars::ui::NoMapBackgroundProvider backgrounds;
	WeatherReportStub weather;

	mars::ui::MainScreen screen{
		lv_screen_active(), {points, missions, routes, backgrounds, weather}, kStartPosition,
	};

	mars::platform::EventLoop::run();
}
