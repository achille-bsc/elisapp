#pragma once

#include <cstdint>

#include <lvgl.h>

#include "core/mission/IMissionRepository.hpp"
#include "core/poi/IPointOfInterestRepository.hpp"
#include "core/route/IRoutePlanner.hpp"
#include "ui/controls/LayerSwitcher.hpp"
#include "ui/controls/MapToolbar.hpp"
#include "ui/map/IMapBackgroundProvider.hpp"
#include "ui/map/MapView.hpp"
#include "ui/panels/MissionDetailSheet.hpp"
#include "ui/panels/MissionListSheet.hpp"
#include "ui/panels/PointOfInterestCard.hpp"

namespace mars::ui {

	/// Told about the requests the screen cannot serve itself.
	class IMainScreenListener {
	public:
		virtual void onWeatherReportRequested(const Mission& mission) = 0;

	protected:
		~IMainScreenListener() = default;
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
			IMainScreenListener& listener;
		};

		/// Builds the widgets in `screen`, which the MainScreen must outlive.
		MainScreen(lv_obj_t* screen, const Services& services, t_position current);
		MainScreen(const MainScreen&) = delete;
		MainScreen& operator=(const MainScreen&) = delete;

		/// Moves the "You" marker and, when a mission is shown, plans its route
		/// again. Call it for every position received.
		void setCurrentPosition(t_position position);

		/// Rebuilds the mission list after the repository has changed.
		void reloadMissions();

		/// Redraws the map picture after new imagery has been received.
		void refreshBackground() { map_.refreshBackground(); }

	private:
		enum class Mode : std::uint8_t {
			Browse,
			MissionList,
			Mission,
			Pinpoint,
		};

		void enter(Mode mode);
		void showMission(const Mission& mission);

		void onMapPressed(t_position position) override;
		void onPointOfInterestPressed(const PointOfInterest& point) override;
		void onLayersPressed() override;
		void onSearchPressed() override;
		void onRecenterPressed() override;
		void onLayerSelected(MapLayer layer) override;
		void onMissionSelected(const Mission& mission) override;
		void onPinpointRequested() override;
		void onMissionClosed() override;
		void onWeatherReportRequested(const Mission& mission) override;
		void onPointOfInterestClosed() override;

		/// Applies the theme to `screen` and returns it.
		static lv_obj_t* prepare(lv_obj_t* screen);
		[[nodiscard]] static lv_obj_t* makeDock(lv_obj_t* screen);

		IMissionRepository& missions_;
		IRoutePlanner& routes_;
		IMainScreenListener& listener_;
		MapView map_;
		lv_obj_t* dock_;
		LayerSwitcher layers_;
		MapToolbar toolbar_;
		MissionListSheet missionList_;
		MissionDetailSheet missionDetail_;
		PointOfInterestCard card_;
		t_position current_;
		Mission pinpoint_;
		/// The mission on screen in Mode::Mission, nullptr otherwise.
		const Mission* active_{nullptr};
		Mode mode_{Mode::Browse};
	};

}
