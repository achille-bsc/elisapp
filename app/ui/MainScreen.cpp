#include "ui/MainScreen.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr const char* kSearchHint = "Search mission destination";
		constexpr const char* kPinpointHint = "Tap the map to choose the destination";

		/// Distance between the toolbar and the bottom of the screen.
		constexpr std::int32_t kToolbarMargin = 18;

		/// The mission standing for a destination picked on the map.
		constexpr Mission kPinpointMission{
			"Pinpoint destination", "Custom", "Destination chosen on the map.", {0, 0}, 0, 0, 0, 0,
		};

	}

	lv_obj_t* MainScreen::prepare(lv_obj_t* screen)
	{
		lv_obj_add_style(screen, &theme::styles().screen, LV_PART_MAIN);
		// The dock reaches below the bottom edge; the screen must not scroll to it.
		lv_obj_set_scrollable(screen, false);
		return screen;
	}

	lv_obj_t* MainScreen::makeDock(lv_obj_t* screen)
	{
		// A column stacked at the bottom of the screen: layer switcher, toolbar,
		// then the open sheet. It reflows by itself when one of them changes.
		lv_obj_t* const dock = widgets::box(screen);
		lv_obj_set_size(dock, lv_pct(100), LV_SIZE_CONTENT);
		lv_obj_set_style_pad_hor(dock, theme::kScreenMargin, LV_PART_MAIN);
		lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(dock, theme::kGap, LV_PART_MAIN);
		return dock;
	}

	MainScreen::MainScreen(lv_obj_t* screen, const Services& services, t_position current)
		: missions_{services.missions}
		, routes_{services.routes}
		, listener_{services.listener}
		, map_{prepare(screen), services.points.all(), current, services.backgrounds, *this, *this}
		, dock_{makeDock(screen)}
		, layers_{dock_, *this}
		, toolbar_{dock_, *this}
		, missionList_{dock_, *this}
		, missionDetail_{dock_, *this}
		, card_{screen, *this}
		, current_{current}
		, pinpoint_{kPinpointMission}
	{
		reloadMissions();
		enter(Mode::Browse);
	}

	void MainScreen::setCurrentPosition(t_position position)
	{
		current_ = position;
		map_.setCurrentPosition(position);
		if (active_ != nullptr) {
			// Cheap with the direct planner; throttle here once a real one is plugged in.
			map_.setRoute(routes_.plan(current_, active_->destination));
		}
	}

	void MainScreen::reloadMissions()
	{
		if (active_ != nullptr && active_ != &pinpoint_) {
			// The mission on screen may be about to disappear.
			enter(Mode::Browse);
		}
		missionList_.setMissions(missions_.all());
	}

	void MainScreen::enter(Mode mode)
	{
		mode_ = mode;
		const bool sheetOpen = mode == Mode::MissionList || mode == Mode::Mission;

		layers_.setVisible(false);
		toolbar_.setCompact(sheetOpen);
		toolbar_.setSearchText(mode == Mode::Pinpoint ? kPinpointHint : kSearchHint);
		missionList_.setVisible(mode == Mode::MissionList);
		if (mode != Mode::Mission) {
			active_ = nullptr;
			missionDetail_.hide();
			map_.clearRoute();
		}
		card_.hide();

		// An open sheet touches the bottom edge, its rounded bottom off screen.
		lv_obj_align(dock_, LV_ALIGN_BOTTOM_MID, 0, sheetOpen ? theme::kSheetRadius : -kToolbarMargin);
	}

	void MainScreen::showMission(const Mission& mission)
	{
		map_.setRoute(routes_.plan(current_, mission.destination));
		missionDetail_.show(mission);
		enter(Mode::Mission);
		active_ = &mission;
	}

	void MainScreen::onMapPressed(t_position position)
	{
		if (mode_ == Mode::Pinpoint) {
			pinpoint_.destination = position;
			showMission(pinpoint_);
			return;
		}
		// A press on the bare map dismisses what floats over it.
		layers_.setVisible(false);
		card_.hide();
	}

	void MainScreen::onPointOfInterestPressed(const PointOfInterest& point)
	{
		if (mode_ == Mode::Pinpoint) {
			onMapPressed(point.position);
			return;
		}
		layers_.setVisible(false);
		card_.show(point);
	}

	void MainScreen::onLayersPressed()
	{
		// The pill opens right above the layers button, wherever the toolbar put it.
		card_.hide();
		layers_.setOffset(toolbar_.layersButtonOffset());
		layers_.setVisible(!layers_.isVisible());
	}

	void MainScreen::onSearchPressed()
	{
		enter(Mode::MissionList);
	}

	void MainScreen::onRecenterPressed()
	{
		map_.centerOnCurrentPosition();
	}

	void MainScreen::onLayerSelected(MapLayer layer)
	{
		map_.setLayer(layer);
		layers_.setVisible(false);
	}

	void MainScreen::onMissionSelected(const Mission& mission)
	{
		showMission(mission);
	}

	void MainScreen::onPinpointRequested()
	{
		enter(Mode::Pinpoint);
	}

	void MainScreen::onMissionClosed()
	{
		enter(Mode::MissionList);
	}

	void MainScreen::onWeatherReportRequested(const Mission& mission)
	{
		listener_.onWeatherReportRequested(mission);
	}

	void MainScreen::onPointOfInterestClosed()
	{
		card_.hide();
	}

}
