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
#include "core/route/TripPlanner.hpp"
#include "core/site/MissionSite.hpp"
#include "core/time/IMissionClock.hpp"
#include "core/weather/StormForecast.hpp"
#include "guidance/course.hpp"
#include "guidance/guidance.hpp"
#include "ui/controls/CompassChip.hpp"
#include "ui/controls/LayerSwitcher.hpp"
#include "ui/controls/MapToolbar.hpp"
#include "ui/controls/map_controls.hpp"
#include "ui/map/IMapBackgroundProvider.hpp"
#include "ui/map/MapView.hpp"
#include "ui/map/render/Legend.hpp"
#include "ui/panels/MissionDetailSheet.hpp"
#include "ui/panels/MissionListSheet.hpp"
#include "ui/panels/PointOfInterestCard.hpp"
#include "ui/panels/guidance_panel.hpp"
#include "ui/panels/weather_card.hpp"

namespace mars::ui {

	/// Told when guidance starts and ends (the PC GPS simulator walks the route).
	class INavigationListener {
	public:
		/// `route` (start first) is valid during the call only.
		virtual void onGuidanceStarted(std::span<const t_position> route, std::uint32_t departureSeconds) = 0;
		virtual void onGuidanceEnded() = 0;

	protected:
		~INavigationListener() = default;
	};

	/// The only screen: the map and everything drawn over it.
	class MainScreen final : private IMapListener,
							 private IPointOfInterestListener,
							 private INavigationPointListener,
							 private IToolbarListener,
							 private ILayerListener,
							 private IMissionListListener,
							 private IMissionDetailListener,
							 private IPointOfInterestCardListener {
	public:
		/// All of it must outlive the screen.
		struct Services {
			IPointOfInterestRepository& points;
			IMissionRepository& missions;
			IRoutePlanner& routes;
			IMapBackgroundProvider& backgrounds;
			const IMapLegendSource& legends;
			const MissionSite& site;
			IEnvironment& environment;
			IStormForecast& storms;
			IPlacePhotos& photos;
			IMissionClock& clock;
			INavigationListener& navigation;
		};

		/// Builds the widgets in `screen`, which the MainScreen must outlive.
		MainScreen(lv_obj_t* screen, const Services& services, t_position current);
		MainScreen(const MainScreen&) = delete;
		MainScreen& operator=(const MainScreen&) = delete;

		/// Call it for every position received.
		void setCurrentPosition(t_position position);

		/// Rebuilds the mission list after the repository has changed.
		void reloadMissions();

		/// Redraws the map picture and storms (time passing).
		void refreshBackground();

		/// False if no such mission.
		bool openMission(std::size_t index);

		/// False if there is no route.
		bool startGuidance();

		void showLayer(MapLayer layer);

		void zoomBy(std::int32_t steps) { map_.zoomBy(steps); }

		/// As a tap on navigation point `index`. False if none.
		bool showNavigationPoint(std::size_t index);

		/// As a tap on the bare map while browsing.
		void showPlaceReport(t_position position);

		void openPlace(const PointOfInterest& point) { onPointOfInterestPressed(point); }

		[[nodiscard]] std::span<const NavigationPoint> navigationPoints() const noexcept { return navigationPoints_; }

		/// The End button: on site it starts the way back, otherwise it ends.
		void endGuidance();

		[[nodiscard]] bool isGuiding() const noexcept { return mode_ == Mode::Guidance; }
		[[nodiscard]] bool isReturning() const noexcept { return returning_; }
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
		void planTrip(t_position destination, std::uint32_t workSeconds);
		void showTrip();
		void placeNavigationPoints(std::span<const t_position> leg, std::uint32_t departureSeconds, std::uint32_t count,
			bool back);
		bool startReturn();
		[[nodiscard]] t_position legTarget() const noexcept;
		void updateGuidance();
		[[nodiscard]] std::uint32_t doseLeft() const noexcept;

		void onMapPressed(t_position position) override;
		void onMapMoved() override;
		void onPointOfInterestPressed(const PointOfInterest& point) override;
		void onNavigationPointPressed(const NavigationPoint& point) override;
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

		static lv_obj_t* prepare(lv_obj_t* screen);
		[[nodiscard]] static lv_obj_t* makeDock(lv_obj_t* screen);

		IMissionRepository& missions_;
		IRoutePlanner& routes_;
		TripPlanner trip_;
		t_position base_;
		IEnvironment& environment_;
		IStormForecast& storms_;
		IPlacePhotos& photos_;
		IMissionClock& clock_;
		INavigationListener& navigation_;
		MapView map_;
		lv_obj_t* dock_;
		LayerSwitcher layers_;
		MapToolbar toolbar_;
		MissionListSheet missionList_;
		MissionDetailSheet missionDetail_;
		PointOfInterestCard card_;
		CompassChip compass_;
		t_map_controls controls_{};
		t_guidance_panel guidancePanel_{};
		t_weather_card weatherCard_{};
		/// Big (the route copy): kept off the stack.
		std::unique_ptr<t_guidance> guidance_;
		t_guidance_fix fix_{};
		t_course course_{};
		t_position current_;
		Mission pinpoint_;
		/// The mission on screen in Mode::Mission and Mode::Guidance, nullptr otherwise.
		const Mission* active_{nullptr};
		/// Copy of the leg on screen or guided.
		std::vector<t_position> route_;
		bool returning_{false};
		std::vector<NavigationPoint> navigationPoints_;
		RouteSummary summary_{};
		t_position plannedFrom_{0, 0};
		std::uint32_t lastReplanSeconds_{0};
		Mode mode_{Mode::Browse};
	};

}
