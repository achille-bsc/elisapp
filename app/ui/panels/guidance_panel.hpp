#pragma once

#include <lvgl.h>

#include <cstdint>

#include "../../core/gps/Position.hpp"
#include "../../guidance/guidance.hpp"

// Turn-by-turn screen, laid out like a road navigator:
//   banner at the top : manoeuvre arrow, distance to it, instruction, and the
//                       next hazard on the way (caution / danger) if any
//   bar at the bottom : time, distance and radiation dose left, arrival time
//                       (LMST at the destination), re-centre and End buttons

namespace mars::ui {

// Structures //
typedef void (*t_panel_cb)(void *context);

struct s_guidance_panel {
  lv_obj_t *banner;
  lv_obj_t *arrow;
  lv_obj_t *distance;
  lv_obj_t *instruction;
  lv_obj_t *hazard;
  lv_obj_t *footer;
  lv_obj_t *summary;
  lv_obj_t *arrival;
  lv_obj_t *recenter;
  lv_obj_t *end_label;
  t_panel_cb on_end;
  t_panel_cb on_recenter;
  void *context;
} typedef t_guidance_panel;

// What the bottom bar shows, beside the fix
struct s_guidance_trip {
  t_position destination;
  uint32_t time_s;          // mission time now
  uint32_t dose_left_usv;   // radiation still to receive on the way
} typedef t_guidance_trip;

// Functions //
void guidance_panel_create(t_guidance_panel *panel, lv_obj_t *parent,
                           t_panel_cb on_end, t_panel_cb on_recenter,
                           void *context);
void guidance_panel_set_visible(t_guidance_panel *panel, bool visible);
void guidance_panel_update(t_guidance_panel *panel, const t_guidance_fix &fix,
                           const t_guidance_trip &trip);
// While the planner looks for a new route from an off-route position
void guidance_panel_show_rerouting(t_guidance_panel *panel);
// No new route: `message` (static string) says why
void guidance_panel_show_blocked(t_guidance_panel *panel, const char *message);
void guidance_panel_show_recenter(t_guidance_panel *panel, bool visible);

} // namespace mars::ui
