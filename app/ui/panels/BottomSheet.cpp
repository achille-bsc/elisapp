#include "ui/panels/BottomSheet.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kPadding = 16;
		/// The whole strip is the touch target of the handle (WCAG 2.5.8: 44 px).
		constexpr std::int32_t kHandleAreaHeight = 44;
		constexpr std::int32_t kHandleWidth = 170;
		constexpr std::int32_t kHandleHeight = 8;

	}

	BottomSheet::BottomSheet(lv_obj_t* parent)
		: root_{widgets::box(parent, &theme::styles().sheet)}
	{
		const theme::Styles& styles = theme::styles();

		lv_obj_set_size(root_, lv_pct(100), LV_SIZE_CONTENT);
		lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_hor(root_, kPadding, LV_PART_MAIN);
		// The sheet swallows presses instead of letting them reach the map.
		lv_obj_set_clickable(root_, true);

		lv_obj_t* const handle = widgets::box(root_);
		lv_obj_set_size(handle, lv_pct(100), kHandleAreaHeight);
		lv_obj_set_clickable(handle, true);
		lv_obj_add_event_cb(handle, onHandle, LV_EVENT_CLICKED, this);

		lv_obj_t* const bar = widgets::box(handle, &styles.pill);
		lv_obj_set_size(bar, kHandleWidth, kHandleHeight);
		lv_obj_center(bar);

		content_ = widgets::box(root_);
		lv_obj_set_size(content_, lv_pct(100), LV_SIZE_CONTENT);
		lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(content_, theme::kGap, LV_PART_MAIN);
		// The bottom corners are rounded too: the owner pushes them off screen.
		lv_obj_set_style_pad_bottom(content_, kPadding + theme::kSheetRadius, LV_PART_MAIN);

		setVisible(false);
	}

	void BottomSheet::setVisible(bool visible)
	{
		widgets::setVisible(root_, visible);
		if (visible) {
			setCollapsed(false);
		}
	}

	void BottomSheet::setCollapsed(bool collapsed)
	{
		widgets::setVisible(content_, !collapsed);
		lv_obj_set_style_pad_bottom(root_, collapsed ? theme::kSheetRadius : 0, LV_PART_MAIN);
	}

	void BottomSheet::onHandle(lv_event_t* event)
	{
		auto* const self = static_cast<BottomSheet*>(lv_event_get_user_data(event));
		self->setCollapsed(!lv_obj_is_hidden(self->content_));
	}

}
