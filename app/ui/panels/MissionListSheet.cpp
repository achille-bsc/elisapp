#include "ui/panels/MissionListSheet.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"
#include "ui/panels/MissionSummary.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kPinpointPadding = 26;
		constexpr std::int32_t kPinpointGap = 18;
		constexpr std::int32_t kRuleHeight = 3;

		/// Three rows and a half: the cut row shows that the list scrolls.
		constexpr std::int32_t kListHeight = 3 * (theme::kTouchTarget + theme::kGap) + theme::kTouchTarget / 2;

		void makeRule(lv_obj_t* parent)
		{
			lv_obj_t* const rule = widgets::box(parent, &theme::styles().pill);
			// Seen on the dark sheet: the panel's line colour, not the pill's
			lv_obj_set_style_bg_color(rule, lv_color_hex(theme::kPanelLine), LV_PART_MAIN);
			lv_obj_set_height(rule, kRuleHeight);
			lv_obj_set_flex_grow(rule, 1);
		}

	}

	MissionListSheet::MissionListSheet(lv_obj_t* parent, IMissionListListener& listener)
		: listener_{listener}
		, sheet_{parent}
	{
		const theme::Styles& styles = theme::styles();
		lv_obj_t* const content = sheet_.content();

		lv_obj_t* const pinpoint = widgets::button(content, &styles.pill, onPinpoint, this);
		lv_obj_set_size(pinpoint, lv_pct(100), theme::kTouchTarget);
		lv_obj_set_style_pad_hor(pinpoint, kPinpointPadding, LV_PART_MAIN);
		widgets::flex(pinpoint, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, kPinpointGap);
		widgets::label(pinpoint, LV_SYMBOL_GPS);
		lv_obj_add_style(widgets::label(pinpoint, "Choose pinpoint destination"), &styles.hint, LV_PART_MAIN);

		lv_obj_t* const title = widgets::box(content);
		lv_obj_set_size(title, lv_pct(100), LV_SIZE_CONTENT);
		widgets::flex(title, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, theme::kGap);
		makeRule(title);
		widgets::label(title, "Houston's scientific target destinations");
		makeRule(title);

		list_ = widgets::box(content);
		lv_obj_set_size(list_, lv_pct(100), kListHeight);
		lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(list_, theme::kGap, LV_PART_MAIN);
		// Clickable so that a drag started between two rows scrolls the list too.
		lv_obj_set_clickable(list_, true);
		lv_obj_set_scrollable(list_, true);
		lv_obj_set_scroll_dir(list_, LV_DIR_VER);
	}

	void MissionListSheet::setMissions(std::span<const Mission> missions)
	{
		lv_obj_clean(list_);
		for (const Mission& mission : missions) {
			lv_obj_t* const row = widgets::button(list_, &theme::styles().pill, onRow, this);
			lv_obj_set_size(row, lv_pct(100), theme::kTouchTarget);
			// LVGL keeps user data as void*; the mission is only ever read back as const.
			lv_obj_set_user_data(row, const_cast<Mission*>(&mission));
			lv_obj_t* const summary = widgets::label(row, "");
			setMissionSummary(summary, mission);
			lv_obj_center(summary);
		}
	}

	void MissionListSheet::onPinpoint(lv_event_t* event)
	{
		static_cast<MissionListSheet*>(lv_event_get_user_data(event))->listener_.onPinpointRequested();
	}

	void MissionListSheet::onRow(lv_event_t* event)
	{
		auto* const self = static_cast<MissionListSheet*>(lv_event_get_user_data(event));
		const auto* const mission = static_cast<const Mission*>(lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
		self->listener_.onMissionSelected(*mission);
	}

}
