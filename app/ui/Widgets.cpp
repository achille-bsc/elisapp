#include "ui/Widgets.hpp"

#include "ui/Theme.hpp"

namespace mars::ui::widgets {

	lv_obj_t* box(lv_obj_t* parent, const lv_style_t* style)
	{
		lv_obj_t* const object = lv_obj_create(parent);
		lv_obj_remove_style_all(object);
		lv_obj_set_clickable(object, false);
		lv_obj_set_scrollable(object, false);
		if (style != nullptr) {
			lv_obj_add_style(object, style, LV_PART_MAIN);
		}
		return object;
	}

	lv_obj_t* button(lv_obj_t* parent, const lv_style_t* style, lv_event_cb_t onClick, void* context,
		const lv_style_t* pressedStyle)
	{
		lv_obj_t* const object = box(parent, style);
		lv_obj_set_clickable(object, true);
		lv_obj_add_style(object, pressedStyle != nullptr ? pressedStyle : &theme::styles().pressed, LV_STATE_PRESSED);
		lv_obj_add_event_cb(object, onClick, LV_EVENT_CLICKED, context);
		return object;
	}

	lv_obj_t* label(lv_obj_t* parent, const char* text)
	{
		lv_obj_t* const object = lv_label_create(parent);
		lv_obj_remove_style_all(object);
		lv_label_set_text_static(object, text);
		return object;
	}

	void flex(lv_obj_t* object, lv_flex_flow_t flow, lv_flex_align_t mainAlign, int gap)
	{
		lv_obj_set_flex_flow(object, flow);
		lv_obj_set_flex_align(object, mainAlign, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_gap(object, gap, LV_PART_MAIN);
	}

	void setVisible(lv_obj_t* object, bool visible)
	{
		lv_obj_set_hidden(object, !visible);
	}

}
