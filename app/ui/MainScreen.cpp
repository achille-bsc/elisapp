#include "ui/MainScreen.hpp"

#include <algorithm>
#include <cstdio>

#include "data/config.hpp"
#include "data/core/geodesy.hpp"
#include "ui/Format.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr const char* kSearchHint = "Search mission destination";
		constexpr const char* kPinpointHint = "Tap the map to choose the destination";

		constexpr std::int32_t kToolbarMargin = 18;

		/// Outside guidance, plan again after moving this far.
		constexpr std::uint32_t kReplanDistanceMm = 200'000;

		constexpr std::uint32_t kReplanCooldownSeconds = 30;

		constexpr Mission kPinpointMission{
			"Pinpoint destination", "Custom", "Destination chosen on the map.", {0, 0}, 0, 0,
		};

		constexpr std::uint32_t kOutboundPoints = 6;
		constexpr std::uint32_t kBackPoints = 5;
		constexpr std::uint32_t kSecondsPerMinute = 60;
		/// Within this of the base, the way back may retrace the way out.
		constexpr std::uint32_t kSameWayMm = 100'000;

		constexpr std::uint32_t kMmPerM = 1'000;

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
		// Bottom column: layer switcher, toolbar, open sheet
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
		, trip_{services.routes, EVA_MAX_DURATION_S, kSameWayMm}
		, base_{services.site.position}
		, environment_{services.environment}
		, storms_{services.storms}
		, photos_{services.photos}
		, clock_{services.clock}
		, navigation_{services.navigation}
		, map_{prepare(screen), services.points.all(), current, services.backgrounds, services.legends, {*this, *this, *this}}
		, dock_{makeDock(screen)}
		, layers_{dock_, *this}
		, toolbar_{dock_, *this}
		, missionList_{dock_, *this}
		, missionDetail_{dock_, *this}
		, card_{screen, *this}
		, compass_{screen}
		, guidance_{std::make_unique<t_guidance>()}
		, current_{current}
		, pinpoint_{kPinpointMission}
	{
		map_controls_create(&controls_, screen, onZoom, this);
		guidance_panel_create(&guidancePanel_, screen, onGuidanceEnd, onGuidanceRecenter, this);
		weather_card_create(&weatherCard_, screen, onWeatherClosed, this);
		reloadMissions();
		enter(Mode::Browse);
		map_.showAround(services.site.position, services.site.viewRadiusM);
		map_.setStorms(storms_.at(clock_.nowSeconds()));
	}

	void MainScreen::refreshBackground()
	{
		map_.setStorms(storms_.at(clock_.nowSeconds()));
		map_.refreshBackground();
	}

	void MainScreen::setCurrentPosition(t_position position)
	{
		current_ = position;
		map_.setCurrentPosition(position);
		const std::uint32_t now = clock_.nowSeconds();
		course_update(&course_, position, now);
		const bool known = course_known(&course_, now);
		map_.setHeading(known, course_.heading_deg);
		compass_.setHeading(known, course_.heading_deg);
		if (mode_ == Mode::Guidance) {
			updateGuidance();
			return;
		}
		if (mode_ == Mode::Mission && active_ != nullptr
			&& coord_distance_mm(toCoord(plannedFrom_), toCoord(position)) > kReplanDistanceMm) {
			planTrip(active_->destination, active_->durationMinutes * kSecondsPerMinute);
			showTrip();
			missionDetail_.setTrip(trip_.summary());
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
			fix_ = {};
			returning_ = false;
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
			navigationPoints_.clear();
		}
		guidance_panel_set_visible(&guidancePanel_, guiding);
		map_.setLegendVisible(!guiding);
		// Under an open sheet the buttons would overlap it; drag and keys still zoom.
		map_controls_set_visible(&controls_, !sheetOpen);
		compass_.setVisible(!sheetOpen && !guiding);
		if (!guiding) {
			map_.setFollow(false);
		}
		card_.hide();
		weather_card_hide(&weatherCard_);

		// An open sheet touches the bottom edge, its rounded bottom off screen.
		lv_obj_align(dock_, LV_ALIGN_BOTTOM_MID, 0, sheetOpen ? theme::kSheetRadius : -kToolbarMargin);
	}

	void MainScreen::planTrip(t_position destination, std::uint32_t workSeconds)
	{
		const std::uint32_t now = clock_.nowSeconds();
		const TripSummary& trip = trip_.plan(current_, destination, base_, now, workSeconds);
		summary_ = trip.outbound;
		route_.assign(trip_.outbound().begin(), trip_.outbound().end());
		returning_ = false;
		plannedFrom_ = current_;
		lastReplanSeconds_ = now;
		if (route_.empty()) {
			route_.push_back(destination);
		}
	}

	void MainScreen::showTrip()
	{
		const TripSummary& trip = trip_.summary();
		navigationPoints_.clear();
		if (returning_) {
			map_.setRoute(route_);
			map_.setReturnRoute({});
			placeNavigationPoints(route_, trip.backDepartureSeconds, kBackPoints, true);
		} else {
			map_.setRoute(route_);
			map_.setReturnRoute(trip_.back());
			placeNavigationPoints(trip_.outbound(), lastReplanSeconds_, kOutboundPoints, false);
			placeNavigationPoints(trip_.back(), trip.backDepartureSeconds, kBackPoints, true);
		}
		map_.setNavigationPoints(navigationPoints_);
	}

	void MainScreen::placeNavigationPoints(std::span<const t_position> leg, std::uint32_t departureSeconds,
		std::uint32_t count, bool back)
	{
		std::array<RouteMark, WaypointLayer::kMaxPoints> marks{};
		const std::size_t room = WaypointLayer::kMaxPoints - std::min(navigationPoints_.size(), WaypointLayer::kMaxPoints);
		const std::size_t found = spreadAlongRoute(leg, count, std::span<RouteMark>{marks}.first(room));
		for (std::size_t k = 0; k < found; ++k) {
			const t_position position = leg[marks[k].vertex];
			const auto eta = static_cast<std::uint32_t>(departureSeconds + marks[k].alongMm / WALK_SPEED_MM_S);
			navigationPoints_.push_back({position, eta, static_cast<std::uint32_t>(marks[k].alongMm / kMmPerM),
				environment_.sample(position, eta).level, static_cast<std::uint8_t>(navigationPoints_.size() + 1), back});
		}
	}

	t_position MainScreen::legTarget() const noexcept
	{
		return returning_ || active_ == nullptr ? base_ : active_->destination;
	}

	bool MainScreen::startReturn()
	{
		const std::uint32_t now = clock_.nowSeconds();
		const TripSummary& trip = trip_.planBack(current_, base_, now);
		// Nothing changes unless the way back can be guided
		if (trip.back.status != RouteStatus::Ok
			|| !guidance_start(guidance_.get(), trip_.back().data(), static_cast<std::uint32_t>(trip_.back().size()),
				WALK_SPEED_MM_S)) {
			guidance_panel_show_blocked(&guidancePanel_, routeStatusMessage(trip.back.status));
			return false;
		}
		summary_ = trip.back;
		route_.assign(trip_.back().begin(), trip_.back().end());
		returning_ = true;
		lastReplanSeconds_ = now;
		fix_ = {};
		showTrip();
		navigation_.onGuidanceStarted(route_, now);
		updateGuidance();
		return true;
	}

	void MainScreen::showMission(const Mission& mission)
	{
		planTrip(mission.destination, mission.durationMinutes * kSecondsPerMinute);
		missionDetail_.show(mission, trip_.summary());
		enter(Mode::Mission);
		active_ = &mission;
		showTrip();
		lv_obj_update_layout(dock_);
		const std::int32_t sheet = lv_obj_get_height(dock_) - theme::kSheetRadius;
		if (route_.size() >= 2) {
			std::vector<t_position> both{route_};
			both.insert(both.end(), trip_.back().begin(), trip_.back().end());
			map_.fitPositions(both, sheet);
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
		// A fix left from an earlier arrival would freeze this guidance
		fix_ = {};
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
			if (returning_) {
				const TripSummary& trip = trip_.planBack(current_, base_, now);
				summary_ = trip.back;
				route_.assign(trip_.back().begin(), trip_.back().end());
				lastReplanSeconds_ = now;
			} else {
				planTrip(active_->destination, active_->durationMinutes * kSecondsPerMinute);
			}
			showTrip();
			if (summary_.status != RouteStatus::Ok
				|| !guidance_start(guidance_.get(), route_.data(), static_cast<std::uint32_t>(route_.size()), WALK_SPEED_MM_S)) {
				guidance_panel_show_blocked(&guidancePanel_, routeStatusMessage(summary_.status));
				return;
			}
			navigation_.onGuidanceStarted(route_, now);
			fix_ = guidance_update(guidance_.get(), current_, now);
		}
		const bool backLeft = !returning_ && trip_.summary().back.status == RouteStatus::Ok;
		const bool known = course_known(&course_, now);
		guidance_panel_update(&guidancePanel_, fix_,
			{legTarget(), now, doseLeft(), returning_, backLeft, known, course_.heading_deg});
		if (fix_.state == GUIDANCE_ARRIVED && before != GUIDANCE_ARRIVED) {
			navigation_.onGuidanceEnded();
		}
	}

	void MainScreen::endGuidance()
	{
		const bool arrived = fix_.state == GUIDANCE_ARRIVED;
		guidance_stop(guidance_.get());
		navigation_.onGuidanceEnded();
		if (arrived && !returning_ && active_ != nullptr) {
			if (!startReturn()) {
				// No way back: the panel says so; End again leaves
				returning_ = true;
			}
			return;
		}
		fix_ = {};
		if (arrived || returning_ || active_ == nullptr) {
			returning_ = false;
			enter(Mode::Browse);
			return;
		}
		returning_ = false;
		// Back to the route overview, planned from where the crew stopped.
		showMission(*active_);
	}

	void MainScreen::onMapPressed(t_position position)
	{
		// A tap on the map closes what floats over it, then the open menu
		const bool floating = layers_.isVisible() || card_.isVisible() || weather_card_is_visible(&weatherCard_);
		layers_.setVisible(false);
		card_.hide();
		weather_card_hide(&weatherCard_);
		if (floating) {
			return;
		}
		if (mode_ == Mode::Pinpoint) {
			pinpoint_.destination = position;
			showMission(pinpoint_);
			return;
		}
		switch (mode_) {
		case Mode::MissionList:
		case Mode::Mission:
			enter(Mode::Browse);
			return;
		case Mode::Browse:
			showPlaceReport(position);
			return;
		default:
			return;
		}
	}

	void MainScreen::showPlaceReport(t_position position)
	{
		card_.hide();
		const std::uint32_t now = clock_.nowSeconds();
		weather_card_show(&weatherCard_, environment_, "This place", position, now, now);
	}

	bool MainScreen::showNavigationPoint(std::size_t index)
	{
		if (index >= navigationPoints_.size()) {
			return false;
		}
		onNavigationPointPressed(navigationPoints_[index]);
		return true;
	}

	void MainScreen::onNavigationPointPressed(const NavigationPoint& point)
	{
		card_.hide();
		layers_.setVisible(false);
		const ShortText along = formatDistance(point.alongM);
		char title[64];
		std::snprintf(title, sizeof title, "%s point %u, %s %s", point.back ? "Return" : "Route",
			static_cast<unsigned>(point.number), along.data(), point.back ? "from the site" : "from the start");
		weather_card_show(&weatherCard_, environment_, title, point.position, point.etaSeconds, clock_.nowSeconds());
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
		card_.show(point, photos_.photo(point));
	}

	void MainScreen::onLayersPressed()
	{
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
		const std::uint32_t now = clock_.nowSeconds();
		char title[96];
		std::snprintf(title, sizeof title, "Weather at %s", mission.name);
		weather_card_show(&weatherCard_, environment_, title, mission.destination, now, now);
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
