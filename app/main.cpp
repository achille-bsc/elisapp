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
#include "core/site/JsonMissionSiteRepository.hpp"
#include "data/sim/weather_mcd.hpp"
#include "integration/DataEnvironment.hpp"
#include "integration/DataStormForecast.hpp"
#include "ui/PhotoLibrary.hpp"
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

	constexpr mars::MissionSite kFallbackSite{
		"Octavia E. Butler Landing", "Perseverance landing site.",
		{mars::tuning::kBaseLatitudeE7, mars::tuning::kBaseLongitudeE7}, 0.0, mars::tuning::kStartLocalHour,
		mars::tuning::kViewRadiusM,
	};

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

	/// Mission time at which the base reads the site's start hour (LMST).
	[[nodiscard]] std::uint32_t startTime(const mars::MissionSite& site) noexcept
	{
		const t_coord base = coord_from_e7_east(site.position.latitudeE7, site.position.longitudeEastE7);
		const std::uint32_t offset = local_solar_time_s(base, 0);
		return (site.startLocalHour * MARS_HOUR_S + MARS_SOL_S - offset) % MARS_SOL_S;
	}

	void loadData(mars::DataSession& session, char* weatherSource, std::size_t weatherSourceSize)
	{
		std::uint32_t relief = 0;
		for (const char* path : mars::tuning::kReliefPaths) {
			relief += session.addReliefLayer(path) ? 1U : 0U;
		}
		if (relief == 0) {
			std::fputs("elisapp: no relief in data/ (run `make data`): every place is unknown terrain\n", stderr);
		}
		std::uint32_t images = 0;
		for (const char* path : mars::tuning::kImageryPaths) {
			images += session.addImageryLayer(path) ? 1U : 0U;
		}
		if (!session.setGround(mars::tuning::kGroundPath)) {
			std::fprintf(stderr, "elisapp: cannot read %s (run `make data`): no soft ground known\n", mars::tuning::kGroundPath);
		}
		std::uint32_t size = 0;
		const void* const weather = session.mapFile(mars::tuning::kWeatherPath, size);
		if (weather != nullptr && weather_mcd_set_blob(weather, size)) {
			std::snprintf(weatherSource, weatherSourceSize, "Mars Climate Database v6.2, Ls %u\xC2\xB0.",
				static_cast<unsigned>((weather_mcd_solar_longitude_ddeg() + 5U) / 10U));
		} else {
			std::fprintf(stderr, "elisapp: cannot read %s (run `make data`): no weather\n", mars::tuning::kWeatherPath);
			std::snprintf(weatherSource, weatherSourceSize, "No weather data loaded.");
		}
		std::printf("elisapp: %u relief and %u image layers loaded\n", static_cast<unsigned>(relief), static_cast<unsigned>(images));
	}

	/// The simulated receiver walks the guided route.
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

	void onGpsTimer(lv_timer_t* timer)
	{
		auto* const loop = static_cast<Loop*>(lv_timer_get_user_data(timer));
		const mars::PositionPacket::Buffer packet = gps_sim_packet(loop->clock.nowSeconds());
		if (const std::optional<mars::t_position> position = mars::PositionPacket::decode(packet)) {
			loop->screen.setCurrentPosition(*position);
		}
	}

	struct Refresh {
		const mars::IMissionClock& clock;
		mars::DataSession& session;
		mars::ui::MainScreen& screen;
	};

	void onBackgroundTimer(lv_timer_t* timer)
	{
		auto* const refresh = static_cast<Refresh*>(lv_timer_get_user_data(timer));
		weather_mcd_set_time(refresh->clock.nowSeconds());
		refresh->session.refreshWeather();
		refresh->screen.refreshBackground();
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

	mars::JsonMissionSiteRepository sites{kFallbackSite};
	if (!sites.load(mars::tuning::kMissionSitePath)) {
		std::fprintf(stderr, "elisapp: cannot read %s, the mission starts at %s\n", mars::tuning::kMissionSitePath,
			kFallbackSite.name);
	}
	const mars::MissionSite& site = sites.site();

	const std::uint32_t start = startTime(site);
	mars::platform::SteadyMissionClock clock{start, options->timeScale};

	mars::DataSession session{weather_sat_mcd};
	weather_mcd_set_time(start);
	static char weatherSource[96];
	loadData(session, weatherSource, sizeof weatherSource);

	mars::JsonPointOfInterestRepository points;
	if (!points.load(mars::tuning::kZonePointsOfInterestPath) && !points.load(mars::tuning::kPointsOfInterestPath)) {
		std::fprintf(stderr, "elisapp: cannot read %s, no point of interest shown\n", mars::tuning::kPointsOfInterestPath);
	}
	mars::JsonMissionRepository missions;
	if (!missions.load(mars::tuning::kZoneMissionsPath) && !missions.load(mars::tuning::kMissionsPath)) {
		std::fprintf(stderr, "elisapp: cannot read %s, no mission proposed\n", mars::tuning::kMissionsPath);
	}
	mars::DataStormForecast storms;
	if (storms.load(mars::tuning::kStormForecastPath, start)) {
		std::printf("elisapp: storm forecast loaded: %s\n", storms.source());
	}
	mars::ui::PhotoLibrary photos{session};
	mars::MarspathRoutePlanner routes;
	mars::DataEnvironment environment;
	mars::ui::data_background backgrounds{clock, weatherSource};

	gps_sim_init(site.position);
	GpsSimNavigation navigation{clock};
	const mars::t_position first = mars::PositionPacket::decode(gps_sim_packet(clock.nowSeconds())).value_or(site.position);

	mars::ui::MainScreen screen{
		lv_screen_active(), {points, missions, routes, backgrounds, backgrounds, site, environment, storms, photos, clock, navigation}, first,
	};
	Loop loop{clock, screen};
	Refresh refresh{clock, session, screen};
	lv_timer_create(onGpsTimer, mars::tuning::kGpsPeriodMs, &loop);
	lv_timer_create(onBackgroundTimer, mars::tuning::kBackgroundRefreshMs, &refresh);

	if (options->demo && (!screen.openMission(0) || !screen.startGuidance())) {
		std::fputs("elisapp: --demo: no route to the first mission\n", stderr);
	}

	mars::platform::EventLoop::run();
}
