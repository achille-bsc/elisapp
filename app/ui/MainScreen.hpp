#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <lvgl.h>

#include "core/environment/IEnvironment.hpp"
#include "core/mission/IMissionRepository.hpp"
#include "core/poi/IPointOfInterestRepository.hpp"
#include "core/route/IRoutePlanner.hpp"
#include "core/time/IMissionClock.hpp"
#include "guidance/guidance.hpp"
#include "ui/controls/LayerSwitcher.hpp"
#include "ui/controls/MapToolbar.hpp"
#include "ui/controls/map_controls.hpp"
#include "ui/map/IMapBackgroundProvider.hpp"
#include "ui/map/MapView.hpp"
#include "ui/panels/MissionDetailSheet.hpp"
#include "ui/panels/MissionListSheet.hpp"
#include "ui/panels/PointOfInterestCard.hpp"
#include "ui/panels/guidance_panel.hpp"
#include "ui/panels/weather_card.hpp"

namespace mars::ui {

	/// Told when turn-by-turn guidance starts and ends: the position source
	/// may need it (the PC simulator walks the route; a real receiver does not).
	class INavigationListener {
	public:
		/// `route` (start first) is valid during the call only.
		virtual void onGuidanceStarted(std::span<const t_position> route, std::uint32_t departureSeconds) = 0;
		virtual void onGuidanceEnded() = 0;

	protected:
		~INavigationListener() = default;
	};

	/// The only screen of the application: the map and everything drawn over it.
	/// It owns the widgets and decides which of them are visible; the data come
	/// from the services given at construction.
	class MainScreen final : private IMapListener,
							 private IPointOfInterestListener,
							 private IToolbarListener,
							 private ILayerListener,
							 private IMissionListListener,
							 private IMissionDetailListener,
							 private IPointOfInterestCardListener {
	public:
		/// Everything the screen depends on. All of it must outlive the screen.
		struct Services {
			IPointOfInterestRepository& points;
			IMissionRepository& missions;
			IRoutePlanner& routes;
			IMapBackgroundProvider& backgrounds;
			IEnvironment& environment;
			IMissionClock& clock;
			INavigationListener& navigation;
		};

		/// Builds the widgets in `screen`, which the MainScreen must outlive.
		MainScreen(lv_obj_t* screen, const Services& services, t_position current);
		MainScreen(const MainScreen&) = delete;
		MainScreen& operator=(const MainScreen&) = delete;

		/// Moves the "You" marker and drives the guidance. Call it for every
		/// position received.
		void setCurrentPosition(t_position position);

		/// Rebuilds the mission list after the repository has changed.
		void reloadMissions();

		/// Redraws the map picture (new imagery, time passing for the weather
		/// and risk layers).
		void refreshBackground() { map_.refreshBackground(); }

		/// Plans the route to mission `index` and shows it. False if no such mission.
		bool openMission(std::size_t index);

		/// Starts turn-by-turn guidance along the route on screen. False if
		/// there is no route.
		bool startGuidance();

		void showLayer(MapLayer layer);

		[[nodiscard]] bool isGuiding() const noexcept { return mode_ == Mode::Guidance; }
		[[nodiscard]] const t_guidance_fix& lastFix() const noexcept { return fix_; }

	private:
		enum class Mode : std::uint8_t {
			Browse,
			MissionList,
			Mission,
			Pinpoint,
			Guidance,
		};

		void enter(Mode mode);
		void showMission(const Mission& mission);
		void planRoute(t_position destination);
		void updateGuidance();
		void endGuidance();
		[[nodiscard]] std::uint32_t doseLeft() const noexcept;

		void onMapPressed(t_position position) override;
		void onMapMoved() override;
		void onPointOfInterestPressed(const PointOfInterest& point) override;
		void onLayersPressed() override;
		void onSearchPressed() override;
		void onRecenterPressed() override;
		void onLayerSelected(MapLayer layer) override;
		void onMissionSelected(const Mission& mission) override;
		void onPinpointRequested() override;
		void onMissionClosed() override;
		void onWeatherReportRequested(const Mission& mission) override;
		void onGuidanceStartRequested() override;
		void onPointOfInterestClosed() override;

		static void onZoom(void* context, std::int32_t steps);
		static void onGuidanceEnd(void* context);
		static void onGuidanceRecenter(void* context);
		static void onWeatherClosed(void* context);

		/// Applies the theme to `screen` and returns it.
		static lv_obj_t* prepare(lv_obj_t* screen);
		[[nodiscard]] static lv_obj_t* makeDock(lv_obj_t* screen);

		IMissionRepository& missions_;
		IRoutePlanner& routes_;
		IEnvironment& environment_;
		IMissionClock& clock_;
		INavigationListener& navigation_;
		MapView map_;
		lv_obj_t* dock_;
		LayerSwitcher layers_;
		MapToolbar toolbar_;
		MissionListSheet missionList_;
		MissionDetailSheet missionDetail_;
		PointOfInterestCard card_;
		t_map_controls controls_{};
		t_guidance_panel guidancePanel_{};
		t_weather_card weatherCard_{};
		/// Big (the route copy): kept off the stack.
		std::unique_ptr<t_guidance> guidance_;
		t_guidance_fix fix_{};
		t_position current_;
		Mission pinpoint_;
		/// The mission on screen in Mode::Mission and Mode::Guidance, nullptr otherwise.
		const Mission* active_{nullptr};
		/// Copy of the last planned route: the planner reuses its buffer.
		std::vector<t_position> route_;
		RouteSummary summary_{};
		t_position plannedFrom_{0, 0};
		std::uint32_t lastReplanSeconds_{0};
		Mode mode_{Mode::Browse};
	};

}
