#pragma once

#include <lvgl.h>

namespace mars::ui::widgets {

	/// A bare container: no theme style, not clickable, not scrollable. Touches
	/// fall through to whatever lies beneath it.
	lv_obj_t* box(lv_obj_t* parent, const lv_style_t* style = nullptr);

	/// A box that reacts to presses and calls `onClick` with `context` as the
	/// event user data. `pressedStyle` is applied while it is held down; the
	/// theme's light "pressed" style by default.
	lv_obj_t* button(lv_obj_t* parent, const lv_style_t* style, lv_event_cb_t onClick, void* context,
		const lv_style_t* pressedStyle = nullptr);

	/// A label showing `text` without copying it: the string must outlive the label.
	lv_obj_t* label(lv_obj_t* parent, const char* text);

	/// Lays the children of `object` out in a row or a column, centred across it.
	void flex(lv_obj_t* object, lv_flex_flow_t flow, lv_flex_align_t mainAlign, int gap);

	void setVisible(lv_obj_t* object, bool visible);

}
