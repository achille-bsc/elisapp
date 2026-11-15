#include "ui/MainScreen.hpp"

#include "data/config.hpp"
#include "data/core/geodesy.hpp"
#include "ui/Format.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr const char* kSearchHint = "Search mission destination";
		constexpr const char* kPinpointHint = "Tap the map to choose the destination";

		/// Distance between the toolbar and the bottom of the screen.
		constexpr std::int32_t kToolbarMargin = 18;

		/// Outside guidance the route is planned again once the crew has
		/// moved this far: the planner takes up to 1.5 s, not for every fix.
		constexpr std::uint32_t kReplanDistanceMm = 200'000;

		/// Off the route, a new route is asked at most this often.
		constexpr std::uint32_t kReplanCooldownSeconds = 30;

		/// The mission standing for a destination picked on the map.
		constexpr Mission kPinpointMission{
			"Pinpoint destination", "Custom", "Destination chosen on the map.", {0, 0}, 0, 0,
		};

		[[nodiscard]] t_coord toCoord(t_position position) noexcept
		{
			return coord_from_e7_east(position.latitudeE7, position.longitudeEastE7);
		}

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
		, environment_{services.environment}
		, clock_{services.clock}
		, navigation_{services.navigation}
		, map_{prepare(screen), services.points.all(), current, services.backgrounds, *this, *this}
		, dock_{makeDock(screen)}
		, layers_{dock_, *this}
		, toolbar_{dock_, *this}
		, missionList_{dock_, *this}
		, missionDetail_{dock_, *this}
		, card_{screen, *this}
		, guidance_{std::make_unique<t_guidance>()}
		, current_{current}
		, pinpoint_{kPinpointMission}
	{
		map_controls_create(&controls_, screen, onZoom, this);
		guidance_panel_create(&guidancePanel_, screen, onGuidanceEnd, onGuidanceRecenter, this);
		weather_card_create(&weatherCard_, screen, onWeatherClosed, this);
		reloadMissions();
		enter(Mode::Browse);
	}

	void MainScreen::setCurrentPosition(t_position position)
	{
		current_ = position;
		map_.setCurrentPosition(position);
		if (mode_ == Mode::Guidance) {
			updateGuidance();
			return;
		}
		if (mode_ == Mode::Mission && active_ != nullptr
			&& coord_distance_mm(toCoord(plannedFrom_), toCoord(position)) > kReplanDistanceMm) {
			planRoute(active_->destination);
			missionDetail_.setRoute(summary_);
		}
	}

	void MainScreen::reloadMissions()
	{
		if (active_ != nullptr && active_ != &pinpoint_) {
			// The mission on screen may be about to disappear.
			if (mode_ == Mode::Guidance) {
				guidance_stop(guidance_.get());
				navigation_.onGuidanceEnded();
			}
			enter(Mode::Browse);
		}
		missionList_.setMissions(missions_.all());
	}

	bool MainScreen::openMission(std::size_t index)
	{
		const std::span<const Mission> all = missions_.all();
		if (index >= all.size()) {
			return false;
		}
		showMission(all[index]);
		return true;
	}

	void MainScreen::showLayer(MapLayer layer)
	{
		layers_.setSelected(layer);
		map_.setLayer(layer);
	}

	void MainScreen::enter(Mode mode)
	{
		mode_ = mode;
		const bool sheetOpen = mode == Mode::MissionList || mode == Mode::Mission;
		const bool guiding = mode == Mode::Guidance;

		layers_.setVisible(false);
		widgets::setVisible(dock_, !guiding);
		toolbar_.setCompact(sheetOpen);
		toolbar_.setSearchText(mode == Mode::Pinpoint ? kPinpointHint : kSearchHint);
		missionList_.setVisible(mode == Mode::MissionList);
		if (mode != Mode::Mission && mode != Mode::Guidance) {
			active_ = nullptr;
			missionDetail_.hide();
			map_.clearRoute();
		}
		guidance_panel_set_visible(&guidancePanel_, guiding);
		// Under an open sheet the buttons would overlap it; drag and keys still zoom.
		map_controls_set_visible(&controls_, !sheetOpen);
		if (!guiding) {
			map_.setFollow(false);
		}
		card_.hide();
		weather_card_hide(&weatherCard_);

		// An open sheet touches the bottom edge, its rounded bottom off screen.
		lv_obj_align(dock_, LV_ALIGN_BOTTOM_MID, 0, sheetOpen ? theme::kSheetRadius : -kToolbarMargin);
	}

	void MainScreen::planRoute(t_position destination)
	{
		const std::uint32_t now = clock_.nowSeconds();
		const Route route = routes_.plan(current_, destination, now);
		summary_ = route.summary;
		route_.assign(route.waypoints.begin(), route.waypoints.end());
		plannedFrom_ = current_;
		lastReplanSeconds_ = now;
		if (route_.empty()) {
			// No way there: the destination is still marked.
			const t_position only[] = {destination};
			map_.setRoute(only);
		} else {
			map_.setRoute(route_);
		}
	}

	void MainScreen::showMission(const Mission& mission)
	{
		planRoute(mission.destination);
		missionDetail_.show(mission, summary_);
		enter(Mode::Mission);
		active_ = &mission;
		// Like a road navigator: the whole way is framed above the sheet.
		lv_obj_update_layout(dock_);
		const std::int32_t sheet = lv_obj_get_height(dock_) - theme::kSheetRadius;
		if (route_.size() >= 2) {
			map_.fitPositions(route_, sheet);
		} else {
			const t_position ends[] = {current_, mission.destination};
			map_.fitPositions(ends, sheet);
		}
	}

	bool MainScreen::startGuidance()
	{
		if (mode_ != Mode::Mission || active_ == nullptr || summary_.status != RouteStatus::Ok
			|| !guidance_start(guidance_.get(), route_.data(), static_cast<std::uint32_t>(route_.size()), WALK_SPEED_MM_S)) {
			return false;
		}
		enter(Mode::Guidance);
		map_.setFollow(true);
		guidance_panel_show_recenter(&guidancePanel_, false);
		navigation_.onGuidanceStarted(route_, clock_.nowSeconds());
		updateGuidance();
		return true;
	}

	std::uint32_t MainScreen::doseLeft() const noexcept
	{
		const std::uint32_t length = guidance_length_mm(guidance_.get());
		if (length == 0) {
			return 0;
		}
		return static_cast<std::uint32_t>(std::uint64_t{summary_.doseMicroSievert} * fix_.remaining_mm / length);
	}

	void MainScreen::updateGuidance()
	{
		const std::uint32_t now = clock_.nowSeconds();
		const std::uint8_t before = fix_.state;
		if (before == GUIDANCE_ARRIVED) {
			return;  // the arrival screen keeps the time it was reached
		}
		fix_ = guidance_update(guidance_.get(), current_, now);
		if (fix_.state == GUIDANCE_OFF_ROUTE) {
			guidance_panel_show_rerouting(&guidancePanel_);
			if (now - lastReplanSeconds_ < kReplanCooldownSeconds) {
				return;
			}
			// Show "Off route" before the planner blocks the loop for a moment.
			lv_refr_now(nullptr);
			planRoute(active_->destination);
			if (summary_.status != RouteStatus::Ok
				|| !guidance_start(guidance_.get(), route_.data(), static_cast<std::uint32_t>(route_.size()), WALK_SPEED_MM_S)) {
				guidance_panel_show_blocked(&guidancePanel_, routeStatusMessage(summary_.status));
				return;
			}
			navigation_.onGuidanceStarted(route_, now);
			fix_ = guidance_update(guidance_.get(), current_, now);
		}
		guidance_panel_update(&guidancePanel_, fix_, {active_->destination, now, doseLeft()});
		if (fix_.state == GUIDANCE_ARRIVED && before != GUIDANCE_ARRIVED) {
			navigation_.onGuidanceEnded();
		}
	}

	void MainScreen::endGuidance()
	{
		const bool arrived = fix_.state == GUIDANCE_ARRIVED;
		guidance_stop(guidance_.get());
		fix_ = {};
		navigation_.onGuidanceEnded();
		if (arrived || active_ == nullptr) {
			enter(Mode::Browse);
			return;
		}
		// Back to the route overview, planned from where the crew stopped.
		showMission(*active_);
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
		weather_card_hide(&weatherCard_);
	}

	void MainScreen::onMapMoved()
	{
		if (mode_ == Mode::Guidance) {
			guidance_panel_show_recenter(&guidancePanel_, true);
		}
	}

	void MainScreen::onPointOfInterestPressed(const PointOfInterest& point)
	{
		if (mode_ == Mode::Pinpoint) {
			onMapPressed(point.position);
			return;
		}
		layers_.setVisible(false);
		weather_card_hide(&weatherCard_);
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
		card_.hide();
		weather_card_show(&weatherCard_, environment_, mission.name, mission.destination, clock_.nowSeconds());
	}

	void MainScreen::onGuidanceStartRequested()
	{
		static_cast<void>(startGuidance());
	}

	void MainScreen::onPointOfInterestClosed()
	{
		card_.hide();
	}

	void MainScreen::onZoom(void* context, std::int32_t steps)
	{
		static_cast<MainScreen*>(context)->map_.zoomBy(steps);
	}

	void MainScreen::onGuidanceEnd(void* context)
	{
		static_cast<MainScreen*>(context)->endGuidance();
	}

	void MainScreen::onGuidanceRecenter(void* context)
	{
		auto* const self = static_cast<MainScreen*>(context);
		self->map_.setFollow(true);
		guidance_panel_show_recenter(&self->guidancePanel_, false);
	}

	void MainScreen::onWeatherClosed(void* context)
	{
		weather_card_hide(&static_cast<MainScreen*>(context)->weatherCard_);
	}

}
