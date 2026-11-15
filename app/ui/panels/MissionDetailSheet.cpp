#include "ui/panels/MissionDetailSheet.hpp"

#include <cstdio>

#include "ui/Format.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"
#include "ui/panels/MissionSummary.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kBackWidth = 96;
		constexpr std::int32_t kActionsWidth = 320;
		constexpr std::int32_t kBodyGap = 24;

		lv_obj_t* makeRow(lv_obj_t* parent, lv_flex_align_t crossAlign, std::int32_t gap)
		{
			lv_obj_t* const row = widgets::box(parent);
			lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
			lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
			lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, crossAlign, LV_FLEX_ALIGN_START);
			lv_obj_set_style_pad_column(row, gap, LV_PART_MAIN);
			return row;
		}

		constexpr std::uint32_t kMicroSievertPerMilli = 1'000;
		constexpr std::uint32_t kMicroSievertPerCentiMilli = 10;

		void setTravelTime(lv_obj_t* label, const char* means, bool known, std::uint32_t seconds)
		{
			if (!known) {
				lv_label_set_text_fmt(label, "%s   --", means);
				return;
			}
			const ShortText time = formatDuration(seconds);
			lv_label_set_text_fmt(label, "%s   %s", means, time.data());
		}

		lv_obj_t* makePillLabel(lv_obj_t* pill, const char* text)
		{
			lv_obj_set_height(pill, theme::kTouchTarget);
			lv_obj_t* const label = widgets::label(pill, text);
			lv_obj_center(label);
			return label;
		}

	}

	MissionDetailSheet::MissionDetailSheet(lv_obj_t* parent, IMissionDetailListener& listener)
		: listener_{listener}
		, sheet_{parent}
	{
		const theme::Styles& styles = theme::styles();

		lv_obj_t* const header = makeRow(sheet_.content(), LV_FLEX_ALIGN_CENTER, theme::kGap);
		lv_obj_t* const back = widgets::button(header, &styles.backButton, onBack, this, &styles.backPressed);
		lv_obj_set_width(back, kBackWidth);
		makePillLabel(back, "Back");
		lv_obj_t* const summary = widgets::box(header, &styles.pill);
		lv_obj_set_flex_grow(summary, 1);
		summary_ = makePillLabel(summary, "");

		lv_obj_t* const body = makeRow(sheet_.content(), LV_FLEX_ALIGN_START, kBodyGap);

		lv_obj_t* const actions = widgets::box(body);
		lv_obj_set_size(actions, kActionsWidth, LV_SIZE_CONTENT);
		lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(actions, theme::kGap, LV_PART_MAIN);

		lv_obj_t* const times = makeRow(actions, LV_FLEX_ALIGN_CENTER, theme::kGap);
		lv_obj_set_flex_flow(times, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(times, theme::kGap, LV_PART_MAIN);
		lv_obj_t* const walk = widgets::box(times, &styles.pill);
		lv_obj_set_width(walk, lv_pct(100));
		walk_ = makePillLabel(walk, "");
		lv_obj_t* const rover = widgets::box(times, &styles.pill);
		lv_obj_set_width(rover, lv_pct(100));
		rover_ = makePillLabel(rover, "");

		start_ = widgets::button(actions, &styles.backButton, onStart, this, &styles.backPressed);
		lv_obj_set_width(start_, lv_pct(100));
		makePillLabel(start_, LV_SYMBOL_PLAY "  Start");

		lv_obj_t* const weather = widgets::button(actions, &styles.pill, onWeather, this);
		lv_obj_set_width(weather, lv_pct(100));
		makePillLabel(weather, LV_SYMBOL_EYE_OPEN "  Weather at the site");

		lv_obj_t* const texts = widgets::box(body);
		lv_obj_set_flex_grow(texts, 1);
		lv_obj_set_height(texts, LV_SIZE_CONTENT);
		lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(texts, theme::kGap, LV_PART_MAIN);
		risk_ = widgets::label(texts, "");
		lv_obj_set_width(risk_, lv_pct(100));
		description_ = widgets::label(texts, "");
		lv_obj_set_width(description_, lv_pct(100));
		lv_obj_add_style(description_, &styles.smallText, LV_PART_MAIN);
	}

	void MissionDetailSheet::show(const Mission& mission, const TripSummary& trip)
	{
		mission_ = &mission;
		setMissionSummary(summary_, mission);
		lv_label_set_text_static(description_, mission.description);
		setTrip(trip);
		sheet_.setVisible(true);
	}

	namespace {

		void describeLeg(char* out, std::size_t size, const char* name, const RouteSummary& leg)
		{
			if (leg.status != RouteStatus::Ok) {
				std::snprintf(out, size, "%s  %s", name, routeStatusMessage(leg.status));
				return;
			}
			const ShortText length = formatDistance(leg.lengthM);
			std::snprintf(out, size, "%s  %s  %s  +%u m", name, hazardLevelName(leg.level), length.data(),
				static_cast<unsigned>(leg.ascentM));
		}

	}

	void MissionDetailSheet::setTrip(const TripSummary& trip)
	{
		const bool out = trip.outbound.status == RouteStatus::Ok;
		const bool back = trip.back.status == RouteStatus::Ok;
		setTravelTime(walk_, "On foot, round trip", out && back, trip.totalSeconds);
		setTravelTime(rover_, "Rover, round trip", out && back,
			trip.outbound.roverSeconds + trip.back.roverSeconds + trip.workSeconds);
		widgets::setVisible(start_, out);
		if (!out) {
			lv_label_set_text_static(risk_, routeStatusMessage(trip.outbound.status));
			return;
		}
		char outText[96];
		char backText[96];
		describeLeg(outText, sizeof outText, LV_SYMBOL_RIGHT " OUT ", trip.outbound);
		describeLeg(backText, sizeof backText, trip.sameWayBack ? LV_SYMBOL_HOME " BACK (same way)" : LV_SYMBOL_HOME " BACK",
			trip.back);
		const ShortText work = formatDuration(trip.workSeconds);
		const LongText reasons = formatHazardReasons(static_cast<HazardReasons>(trip.outbound.reasons | trip.back.reasons));
		lv_label_set_text_fmt(risk_, "%s\n%s\nWork on site %s  \xE2\x80\xA2  dose %u.%02u mSv\n%s%s%s",
			outText, backText, work.data(), static_cast<unsigned>(trip.doseMicroSievert / kMicroSievertPerMilli),
			static_cast<unsigned>(trip.doseMicroSievert % kMicroSievertPerMilli / kMicroSievertPerCentiMilli),
			!back             ? LV_SYMBOL_WARNING " No way back to the base from the site."
			: trip.fitsOneEva ? "Round trip within one EVA (8 h)."
							  : LV_SYMBOL_WARNING " Longer than one EVA (8 h): take the rover.",
			reasons[0] != '\0' ? "\n" LV_SYMBOL_WARNING " " : "", reasons.data());
	}

	void MissionDetailSheet::hide()
	{
		mission_ = nullptr;
		sheet_.setVisible(false);
	}

	void MissionDetailSheet::onBack(lv_event_t* event)
	{
		static_cast<MissionDetailSheet*>(lv_event_get_user_data(event))->listener_.onMissionClosed();
	}

	void MissionDetailSheet::onStart(lv_event_t* event)
	{
		static_cast<MissionDetailSheet*>(lv_event_get_user_data(event))->listener_.onGuidanceStartRequested();
	}

	void MissionDetailSheet::onWeather(lv_event_t* event)
	{
		auto* const self = static_cast<MissionDetailSheet*>(lv_event_get_user_data(event));
		if (self->mission_ != nullptr) {
			self->listener_.onWeatherReportRequested(*self->mission_);
		}
	}

}
