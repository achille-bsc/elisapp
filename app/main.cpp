#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>

#include <lvgl.h>

#include "core/gps/packet/MarsPositionPacket.hpp"
#include "core/mission/JsonMissionRepository.hpp"
#include "core/poi/JsonPointOfInterestRepository.hpp"
#include "data/data.hpp"
#include "data/sim/weather_sim.hpp"
#include "integration/DataEnvironment.hpp"
#include "integration/DataSession.hpp"
#include "integration/MarspathRoutePlanner.hpp"
#include "integration/Tuning.hpp"
#include "platform/EventLoop.hpp"
#include "platform/ScreenGeometry.hpp"
#include "platform/SdlDisplayBackend.hpp"
#include "platform/SteadyMissionClock.hpp"
#include "platform/gps_sim.hpp"
#include "ui/MainScreen.hpp"
#include "ui/map/data_background.hpp"

namespace {

	constexpr mars::t_position kBase{mars::tuning::kBaseLatitudeE7, mars::tuning::kBaseLongitudeE7};

	constexpr const char* kUsage =
		"usage: elisapp [--time-scale N] [--demo]\n"
		"  --time-scale N  the mission clock runs N times faster (rehearse a traverse)\n"
		"  --demo          open the first mission and start the guidance\n";

	struct Options {
		std::uint32_t timeScale{1};
		bool demo{false};
	};

	[[nodiscard]] std::optional<Options> parseOptions(int argc, char** argv)
	{
		Options options;
		for (int index = 1; index < argc; ++index) {
			const char* const argument = argv[index];
			if (std::strcmp(argument, "--demo") == 0) {
				options.demo = true;
			} else if (std::strcmp(argument, "--time-scale") == 0 && index + 1 < argc) {
				const char* const value = argv[++index];
				const char* const end = value + std::strlen(value);
				const auto [parsed, error] = std::from_chars(value, end, options.timeScale);
				if (error != std::errc{} || parsed != end || options.timeScale == 0) {
					return std::nullopt;
				}
			} else {
				return std::nullopt;
			}
		}
		return options;
	}

	/// Mission time at which the base reads kStartLocalHour LMST.
	[[nodiscard]] std::uint32_t startTime() noexcept
	{
		const t_coord base = coord_from_e7_east(kBase.latitudeE7, kBase.longitudeEastE7);
		const std::uint32_t offset = local_solar_time_s(base, 0);
		return (mars::tuning::kStartLocalHour * MARS_HOUR_S + MARS_SOL_S - offset) % MARS_SOL_S;
	}

	/// Under guidance the simulated receiver walks the route; it stops when
	/// the guidance ends. With a real receiver this listener does nothing.
	class GpsSimNavigation final : public mars::ui::INavigationListener {
	public:
		explicit GpsSimNavigation(const mars::IMissionClock& clock) noexcept
			: clock_{clock}
		{
		}

		void onGuidanceStarted(std::span<const mars::t_position> route, std::uint32_t departureSeconds) override
		{
			gps_sim_follow(route.data(), static_cast<std::uint32_t>(route.size()), WALK_SPEED_MM_S, departureSeconds);
		}

		void onGuidanceEnded() override { gps_sim_stop(clock_.nowSeconds()); }

	private:
		const mars::IMissionClock& clock_;
	};

	struct Loop {
		const mars::IMissionClock& clock;
		mars::ui::MainScreen& screen;
	};

	/// Reads the receiver's packet and hands the decoded position to the screen.
	void onGpsTimer(lv_timer_t* timer)
	{
		auto* const loop = static_cast<Loop*>(lv_timer_get_user_data(timer));
		const mars::PositionPacket::Buffer packet = gps_sim_packet(loop->clock.nowSeconds());
		if (const std::optional<mars::t_position> position = mars::PositionPacket::decode(packet)) {
			loop->screen.setCurrentPosition(*position);
		}
	}

	void onBackgroundTimer(lv_timer_t* timer)
	{
		static_cast<mars::ui::MainScreen*>(lv_timer_get_user_data(timer))->refreshBackground();
	}

}

int main(int argc, char** argv)
{
	const std::optional<Options> options = parseOptions(argc, argv);
	if (!options) {
		std::fputs(kUsage, stderr);
		return EXIT_FAILURE;
	}

	lv_init();
	mars::platform::EventLoop::installTick();

	mars::platform::SdlDisplayBackend display;
	if (!display.open(mars::platform::kIpadMiniLandscape, "Elisapp - Mars GPS")) {
		std::fputs("elisapp: cannot open the display\n", stderr);
		return EXIT_FAILURE;
	}

	const std::uint32_t start = startTime();
	mars::platform::SteadyMissionClock clock{start, options->timeScale};

	mars::DataSession session{weather_sat_sim};
	// The simulated satellite stamps its forecasts with the start time.
	weather_sim_set_time(start);
	if (!session.loadLandform(mars::tuning::kLandformPath)) {
		std::fprintf(stderr, "elisapp: cannot read %s (run `make data`): every place is unknown terrain\n",
			mars::tuning::kLandformPath);
	} else if (session.addLandformLayer(mars::tuning::kFineLandformPath)) {
		std::printf("elisapp: fine relief layer %s loaded over MOLA\n", mars::tuning::kFineLandformPath);
	}

	mars::JsonPointOfInterestRepository points;
	if (!points.load(mars::tuning::kPointsOfInterestPath)) {
		std::fprintf(stderr, "elisapp: cannot read %s, no point of interest shown\n", mars::tuning::kPointsOfInterestPath);
	}
	mars::JsonMissionRepository missions;
	if (!missions.load(mars::tuning::kMissionsPath)) {
		std::fprintf(stderr, "elisapp: cannot read %s, no mission proposed\n", mars::tuning::kMissionsPath);
	}
	mars::MarspathRoutePlanner routes;
	mars::DataEnvironment environment;
	mars::ui::data_background backgrounds{clock};

	gps_sim_init(kBase);
	GpsSimNavigation navigation{clock};
	const mars::t_position first = mars::PositionPacket::decode(gps_sim_packet(clock.nowSeconds())).value_or(kBase);

	mars::ui::MainScreen screen{
		lv_screen_active(), {points, missions, routes, backgrounds, environment, clock, navigation}, first,
	};
	Loop loop{clock, screen};
	lv_timer_create(onGpsTimer, mars::tuning::kGpsPeriodMs, &loop);
	lv_timer_create(onBackgroundTimer, mars::tuning::kBackgroundRefreshMs, &screen);

	if (options->demo && (!screen.openMission(0) || !screen.startGuidance())) {
		std::fputs("elisapp: --demo: no route to the first mission\n", stderr);
	}

	mars::platform::EventLoop::run();
}
