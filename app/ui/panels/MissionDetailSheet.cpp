#include "ui/panels/MissionDetailSheet.hpp"

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
		/// The dose is shown with two decimals of mSv.
		constexpr std::uint32_t kMicroSievertPerCentiMilli = 10;

		/// No route means no travel time.
		void setTravelTime(lv_obj_t* label, const char* means, bool known, std::uint32_t seconds)
		{
			if (!known) {
				lv_label_set_text_fmt(label, "%s   --", means);
				return;
			}
			const ShortText time = formatDuration(seconds);
			lv_label_set_text_fmt(label, "%s   %s", means, time.data());
		}

		/// A pill with a centred label, which is returned.
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
		lv_obj_t* const walk = widgets::box(times, &styles.pill);
		lv_obj_set_flex_grow(walk, 1);
		walk_ = makePillLabel(walk, "");
		lv_obj_t* const rover = widgets::box(times, &styles.pill);
		lv_obj_set_flex_grow(rover, 1);
		rover_ = makePillLabel(rover, "");

		start_ = widgets::button(actions, &styles.backButton, onStart, this, &styles.backPressed);
		lv_obj_set_width(start_, lv_pct(100));
		makePillLabel(start_, LV_SYMBOL_PLAY "  Start");

		lv_obj_t* const weather = widgets::button(actions, &styles.pill, onWeather, this);
		lv_obj_set_width(weather, lv_pct(100));
		makePillLabel(weather, "Weather Report");

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

	void MissionDetailSheet::show(const Mission& mission, const RouteSummary& route)
	{
		mission_ = &mission;
		setMissionSummary(summary_, mission);
		lv_label_set_text_static(description_, mission.description);
		setRoute(route);
		sheet_.setVisible(true);
	}

	void MissionDetailSheet::setRoute(const RouteSummary& route)
	{
		const bool ok = route.status == RouteStatus::Ok;
		setTravelTime(walk_, "Walk", ok, route.walkSeconds);
		setTravelTime(rover_, "Rover", ok, route.roverSeconds);
		widgets::setVisible(start_, ok);
		if (!ok) {
			lv_label_set_text_static(risk_, routeStatusMessage(route.status));
			return;
		}
		const ShortText length = formatDistance(route.lengthM);
		const LongText reasons = formatHazardReasons(route.reasons);
		lv_label_set_text_fmt(risk_, "%s   %s   +%u m   %u.%02u mSv%s%s", hazardLevelName(route.level), length.data(),
			static_cast<unsigned>(route.ascentM), static_cast<unsigned>(route.doseMicroSievert / kMicroSievertPerMilli),
			static_cast<unsigned>(route.doseMicroSievert % kMicroSievertPerMilli / kMicroSievertPerCentiMilli),
			reasons[0] != '\0' ? "\n" : "", reasons.data());
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
