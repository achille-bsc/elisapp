#pragma once

#include <lvgl.h>

#include <cstdint>

// Zoom buttons over the right edge of the map: "+" above "-". The map is
// also dragged with the finger or the mouse, and driven by the keyboard
// (arrows, + and -): see MapView.

namespace mars::ui {

// Structures //
typedef void (*t_zoom_cb)(void *context, int32_t steps);

struct s_map_controls {
  lv_obj_t *root;
  t_zoom_cb on_zoom;
  void *context;
} typedef t_map_controls;

// Functions //
void map_controls_create(t_map_controls *controls, lv_obj_t *parent,
                         t_zoom_cb on_zoom, void *context);
void map_controls_set_visible(t_map_controls *controls, bool visible);

} // namespace mars::ui
