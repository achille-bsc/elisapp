#pragma once

#include <lvgl.h>

#include <cstdint>

#include "../../core/gps/Position.hpp"
#include "../../guidance/guidance.hpp"

// Guidance screen: rotation to make in degrees, heading (HDG) and bearing
// (BRG) from true north, next turn, next hazard; distance, time and dose left.

namespace mars::ui {

// Structures //
typedef void (*t_panel_cb)(void *context);

struct s_guidance_panel {
  lv_obj_t *banner;
  lv_obj_t *dial;
  lv_obj_t *dial_value;
  lv_obj_t *dial_caption;
  lv_obj_t *command;
  lv_obj_t *headings;
  lv_obj_t *next;
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

struct s_guidance_trip {
  t_position destination;
  uint32_t time_s;          // mission time now
  uint32_t dose_left_usv;   // radiation still to receive on the way
  bool returning;           // this leg goes back to the base
  bool back_next;           // the way back follows this leg
  bool heading_known;       // heading_deg is measured (the crew moves)
  uint16_t heading_deg;     // course over ground, from true north
} typedef t_guidance_trip;

// Functions //
void guidance_panel_create(t_guidance_panel *panel, lv_obj_t *parent,
                           t_panel_cb on_end, t_panel_cb on_recenter,
                           void *context);
void guidance_panel_set_visible(t_guidance_panel *panel, bool visible);
void guidance_panel_update(t_guidance_panel *panel, const t_guidance_fix &fix,
                           const t_guidance_trip &trip);
void guidance_panel_show_rerouting(t_guidance_panel *panel);
// No new route: `message` (static string) says why
void guidance_panel_show_blocked(t_guidance_panel *panel, const char *message);
void guidance_panel_show_recenter(t_guidance_panel *panel, bool visible);
// "R 35°", "L 120°", "0°" (> 0 = right)
void guidance_format_turn(char *out, uint32_t size, int32_t turn_deg);

} // namespace mars::ui
