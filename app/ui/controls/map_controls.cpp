#include "./map_controls.hpp"

#include "../Theme.hpp"
#include "../Widgets.hpp"

namespace mars::ui {

// Constants //
constexpr int32_t ZOOM_IN_STEPS = 1;
constexpr int32_t ZOOM_OUT_STEPS = -1;

static void on_zoom_in(lv_event_t *event) {
  auto *const controls =
      static_cast<t_map_controls *>(lv_event_get_user_data(event));
  controls->on_zoom(controls->context, ZOOM_IN_STEPS);
}

static void on_zoom_out(lv_event_t *event) {
  auto *const controls =
      static_cast<t_map_controls *>(lv_event_get_user_data(event));
  controls->on_zoom(controls->context, ZOOM_OUT_STEPS);
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *symbol,
                             lv_event_cb_t on_click, void *context) {
  lv_obj_t *const button =
      widgets::button(parent, &theme::styles().pill, on_click, context);
  lv_obj_set_size(button, theme::kTouchTarget, theme::kTouchTarget);
  lv_obj_center(widgets::label(button, symbol));
  return (button);
}

void map_controls_create(t_map_controls *controls, lv_obj_t *parent,
                         t_zoom_cb on_zoom, void *context) {
  controls->on_zoom = on_zoom;
  controls->context = context;
  controls->root = widgets::box(parent);
  lv_obj_set_size(controls->root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  widgets::flex(controls->root, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_CENTER,
                theme::kGap);
  lv_obj_align(controls->root, LV_ALIGN_RIGHT_MID, -theme::kScreenMargin, 0);
  make_button(controls->root, LV_SYMBOL_PLUS, on_zoom_in, controls);
  make_button(controls->root, LV_SYMBOL_MINUS, on_zoom_out, controls);
}

void map_controls_set_visible(t_map_controls *controls, bool visible) {
  widgets::setVisible(controls->root, visible);
}

static_assert(ZOOM_IN_STEPS == -ZOOM_OUT_STEPS);

} // namespace mars::ui
