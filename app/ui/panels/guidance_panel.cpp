#include "./guidance_panel.hpp"

#include <cstdio>
#include <cstdlib>

#include "../../data/data.hpp"
#include "../Format.hpp"
#include "../Theme.hpp"
#include "../Widgets.hpp"
#include "../mission_time.hpp"

namespace mars::ui {

// Constants //
constexpr uint32_t PANEL_COLOR = 0x0B1016;
constexpr uint32_t PANEL_LINE = 0x2A3440;
constexpr uint32_t TEXT_COLOR = 0xFFFFFF;
constexpr uint32_t DIM_COLOR = 0x8A97A6;
constexpr uint32_t ON_COURSE_COLOR = 0x3DDC97;
constexpr uint32_t TURN_COLOR = 0x4FC3F7;
constexpr uint32_t CAUTION_COLOR = 0xF0B429;
constexpr uint32_t DANGER_COLOR = 0xE5484D;
constexpr lv_opa_t PANEL_OPA = 235;
constexpr int32_t BANNER_WIDTH = 780;
constexpr int32_t BANNER_PADDING = 16;
constexpr int32_t PANEL_RADIUS = 18;
constexpr int32_t DIAL_SIZE = 132;
constexpr int32_t DIAL_WIDTH = 10;
constexpr int32_t FOOTER_PADDING = 14;
constexpr int32_t BUTTON_WIDTH = 190;
constexpr int32_t DEG_PER_TURN_I = 360;
// Within this many degrees the crew is on course
constexpr int32_t ON_COURSE_DEG = 8;
constexpr uint32_t MM_PER_M = 1000;
constexpr uint32_t USV_PER_MSV = 1000;
constexpr uint32_t USV_PER_CENTI_MSV = 10;
constexpr uint32_t TEXT_SIZE = 96;

static void on_end(lv_event_t *event) {
  auto *const panel = static_cast<t_guidance_panel *>(lv_event_get_user_data(event));
  panel->on_end(panel->context);
}

static void on_recenter(lv_event_t *event) {
  auto *const panel = static_cast<t_guidance_panel *>(lv_event_get_user_data(event));
  panel->on_recenter(panel->context);
}

void guidance_format_turn(char *out, uint32_t size, int32_t turn_deg) {
  const int32_t magnitude = turn_deg < 0 ? -turn_deg : turn_deg;
  if (magnitude == 0)
    std::snprintf(out, size, "0\xC2\xB0");
  else
    std::snprintf(out, size, "%c %d\xC2\xB0", turn_deg > 0 ? 'R' : 'L',
                  static_cast<int>(magnitude));
}

static lv_obj_t *panel_box(lv_obj_t *parent) {
  lv_obj_t *const box = widgets::box(parent);
  lv_obj_set_style_bg_color(box, lv_color_hex(PANEL_COLOR), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(box, PANEL_OPA, LV_PART_MAIN);
  lv_obj_set_style_radius(box, PANEL_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_border_color(box, lv_color_hex(PANEL_LINE), LV_PART_MAIN);
  lv_obj_set_style_border_width(box, 1, LV_PART_MAIN);
  lv_obj_set_style_text_color(box, lv_color_hex(TEXT_COLOR), LV_PART_MAIN);
  lv_obj_set_clickable(box, true); // swallows presses
  return (box);
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color) {
  lv_obj_t *const label = widgets::label(parent, "");
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  return (label);
}

static lv_obj_t *make_button(lv_obj_t *parent, uint32_t color, lv_event_cb_t cb,
                             void *context, const char *caption,
                             lv_obj_t **label) {
  lv_obj_t *const button = widgets::button(parent, nullptr, cb, context);
  lv_obj_set_size(button, BUTTON_WIDTH, theme::kTouchTarget);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(button, theme::kTouchTarget / 2, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);
  lv_obj_t *const text_label = widgets::label(button, caption);
  lv_obj_set_style_text_color(text_label, lv_color_hex(TEXT_COLOR), LV_PART_MAIN);
  lv_obj_center(text_label);
  if (label)
    *label = text_label;
  return (button);
}

void guidance_panel_create(t_guidance_panel *panel, lv_obj_t *parent,
                           t_panel_cb on_end_cb, t_panel_cb on_recenter_cb,
                           void *context) {
  panel->on_end = on_end_cb;
  panel->on_recenter = on_recenter_cb;
  panel->context = context;

  panel->banner = panel_box(parent);
  lv_obj_set_size(panel->banner, BANNER_WIDTH, LV_SIZE_CONTENT);
  lv_obj_align(panel->banner, LV_ALIGN_TOP_MID, 0, theme::kScreenMargin / 2);
  lv_obj_set_style_pad_all(panel->banner, BANNER_PADDING, LV_PART_MAIN);
  widgets::flex(panel->banner, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, theme::kGap * 2);

  // 0 at the top, clockwise for a right turn
  panel->dial = lv_arc_create(panel->banner);
  lv_obj_set_size(panel->dial, DIAL_SIZE, DIAL_SIZE);
  lv_obj_set_clickable(panel->dial, false);
  lv_obj_remove_style(panel->dial, nullptr, LV_PART_KNOB);
  lv_arc_set_rotation(panel->dial, 270);
  lv_arc_set_bg_angles(panel->dial, 0, DEG_PER_TURN_I);
  lv_obj_set_style_arc_width(panel->dial, DIAL_WIDTH, LV_PART_MAIN);
  lv_obj_set_style_arc_width(panel->dial, DIAL_WIDTH, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(panel->dial, lv_color_hex(PANEL_LINE), LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(panel->dial, false, LV_PART_INDICATOR);
  lv_obj_t *const centre = widgets::box(panel->dial);
  lv_obj_set_size(centre, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_center(centre);
  widgets::flex(centre, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_CENTER, 0);
  panel->dial_value = text(centre, &lv_font_montserrat_24, TEXT_COLOR);
  panel->dial_caption = text(centre, &lv_font_montserrat_14, DIM_COLOR);

  lv_obj_t *const texts = widgets::box(panel->banner);
  lv_obj_set_flex_grow(texts, 1);
  lv_obj_set_height(texts, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(texts, theme::kGap / 2, LV_PART_MAIN);
  panel->command = text(texts, &lv_font_montserrat_36, TEXT_COLOR);
  panel->headings = text(texts, &lv_font_montserrat_18, TEXT_COLOR);
  panel->next = text(texts, &lv_font_montserrat_18, DIM_COLOR);
  lv_obj_set_width(panel->next, lv_pct(100));
  panel->hazard = text(texts, &lv_font_montserrat_14, PANEL_COLOR);
  lv_obj_set_width(panel->hazard, lv_pct(100));
  lv_obj_set_style_bg_opa(panel->hazard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(panel->hazard, theme::kGap / 2, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel->hazard, theme::kGap / 2, LV_PART_MAIN);

  panel->footer = panel_box(parent);
  lv_obj_set_size(panel->footer, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_align(panel->footer, LV_ALIGN_BOTTOM_MID, 0, PANEL_RADIUS);
  lv_obj_set_style_pad_all(panel->footer, FOOTER_PADDING, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(panel->footer, FOOTER_PADDING + PANEL_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(panel->footer, theme::kScreenMargin, LV_PART_MAIN);
  widgets::flex(panel->footer, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, theme::kGap);

  lv_obj_t *const info = widgets::box(panel->footer);
  lv_obj_set_flex_grow(info, 1);
  lv_obj_set_height(info, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  panel->summary = text(info, &lv_font_montserrat_24, TEXT_COLOR);
  panel->arrival = text(info, &lv_font_montserrat_14, DIM_COLOR);

  panel->recenter = make_button(panel->footer, PANEL_LINE, on_recenter, panel,
                                LV_SYMBOL_GPS "  Re-centre", nullptr);
  make_button(panel->footer, DANGER_COLOR, on_end, panel, "End", &panel->end_label);

  guidance_panel_show_recenter(panel, false);
  guidance_panel_set_visible(panel, false);
}

void guidance_panel_set_visible(t_guidance_panel *panel, bool visible) {
  widgets::setVisible(panel->banner, visible);
  widgets::setVisible(panel->footer, visible);
}

void guidance_panel_show_recenter(t_guidance_panel *panel, bool visible) {
  widgets::setVisible(panel->recenter, visible);
}

static void set_dial(t_guidance_panel *panel, int32_t degrees, uint32_t color) {
  const int32_t d = degrees % DEG_PER_TURN_I;
  if (d >= 0)
    lv_arc_set_angles(panel->dial, 0, static_cast<lv_value_precise_t>(d));
  else
    lv_arc_set_angles(panel->dial, static_cast<lv_value_precise_t>(DEG_PER_TURN_I + d),
                      DEG_PER_TURN_I);
  lv_obj_set_style_arc_color(panel->dial, lv_color_hex(color), LV_PART_INDICATOR);
}

static void show_hazard(t_guidance_panel *panel, const t_guidance_fix &fix) {
  if (fix.hazard_level == HAZARD_SAFE) {
    widgets::setVisible(panel->hazard, false);
    return;
  }
  const bool danger = fix.hazard_level == HAZARD_DANGER;
  const ShortText where = formatDistance(fix.to_hazard_mm / MM_PER_M);
  const LongText why = formatHazardReasons(fix.hazard_reasons);
  lv_obj_set_style_bg_color(panel->hazard,
                            lv_color_hex(danger ? DANGER_COLOR : CAUTION_COLOR),
                            LV_PART_MAIN);
  if (fix.to_hazard_mm == 0)
    lv_label_set_text_fmt(panel->hazard, LV_SYMBOL_WARNING " %s HERE: %s",
                          danger ? "DANGER" : "CAUTION", why.data());
  else
    lv_label_set_text_fmt(panel->hazard, LV_SYMBOL_WARNING " %s IN %s: %s",
                          danger ? "DANGER" : "CAUTION", where.data(), why.data());
  widgets::setVisible(panel->hazard, true);
}

static void show_footer(t_guidance_panel *panel, const t_guidance_fix &fix,
                        const t_guidance_trip &trip) {
  const ShortText left_time = formatDuration(fix.remaining_s);
  const ShortText left_distance = formatDistance(fix.remaining_mm / MM_PER_M);
  const ShortText arrival =
      format_lmst(trip.destination, trip.time_s + fix.remaining_s);
  lv_label_set_text_fmt(panel->summary, "%s  \xE2\x80\xA2  %s", left_distance.data(),
                        left_time.data());
  lv_label_set_text_fmt(panel->arrival, "%s  \xE2\x80\xA2  ETA %s  \xE2\x80\xA2  DOSE %u.%02u mSv",
                        trip.returning ? "LEG 2/2  BACK TO BASE"
                                       : (trip.back_next ? "LEG 1/2  OUT" : "OUT"),
                        arrival.data(),
                        static_cast<unsigned>(trip.dose_left_usv / USV_PER_MSV),
                        static_cast<unsigned>(trip.dose_left_usv % USV_PER_MSV /
                                              USV_PER_CENTI_MSV));
}

static void show_arrived(t_guidance_panel *panel, const t_guidance_trip &trip) {
  set_dial(panel, DEG_PER_TURN_I - 1, ON_COURSE_COLOR);
  lv_label_set_text_static(panel->dial_value, LV_SYMBOL_OK);
  lv_label_set_text_static(panel->dial_caption, "ARRIVED");
  if (trip.returning) {
    lv_label_set_text_static(panel->command, LV_SYMBOL_HOME "  AT BASE");
    lv_label_set_text_static(panel->headings, "The crew is back at the base.");
    lv_label_set_text_static(panel->end_label, "Done");
  } else if (trip.back_next) {
    lv_label_set_text_static(panel->command, "ON SITE");
    lv_label_set_text_static(panel->headings, "Do the work, then head back to the base.");
    lv_label_set_text_static(panel->end_label, LV_SYMBOL_HOME "  Return to base");
  } else {
    lv_label_set_text_static(panel->command, "ON SITE");
    lv_label_set_text_static(panel->headings, "The destination is here.");
    lv_label_set_text_static(panel->end_label, "Done");
  }
  lv_label_set_text_static(panel->next, "");
}

void guidance_panel_update(t_guidance_panel *panel, const t_guidance_fix &fix,
                           const t_guidance_trip &trip) {
  guidance_panel_set_visible(panel, true);
  show_footer(panel, fix, trip);
  lv_label_set_text_static(panel->end_label, "End");
  if (fix.state == GUIDANCE_ARRIVED) {
    show_arrived(panel, trip);
    show_hazard(panel, fix);
    return;
  }
  if (fix.state == GUIDANCE_OFF_ROUTE) {
    guidance_panel_show_rerouting(panel);
    return;
  }
  lv_obj_set_style_border_color(panel->banner, lv_color_hex(PANEL_LINE), LV_PART_MAIN);
  char value[24];
  char line[TEXT_SIZE];
  const uint16_t bearing = fix.heading_deg;
  if (trip.heading_known) {
    const int32_t turn = guidance_turn_deg(trip.heading_deg, bearing);
    const bool on_course = std::abs(turn) <= ON_COURSE_DEG;
    guidance_format_turn(value, sizeof value, turn);
    set_dial(panel, turn, on_course ? ON_COURSE_COLOR : TURN_COLOR);
    lv_label_set_text(panel->dial_value, value);
    lv_label_set_text_static(panel->dial_caption, on_course ? "ON COURSE" : "ROTATE");
    if (on_course)
      std::snprintf(line, sizeof line, "HOLD %03u\xC2\xB0", static_cast<unsigned>(bearing));
    else
      std::snprintf(line, sizeof line, "ROTATE %s", value);
    lv_label_set_text(panel->command, line);
    lv_label_set_text_fmt(panel->headings, "HDG %03u\xC2\xB0   " LV_SYMBOL_RIGHT "   BRG %03u\xC2\xB0 %s",
                          static_cast<unsigned>(trip.heading_deg),
                          static_cast<unsigned>(bearing), guidance_compass(bearing));
  } else {
    // No heading measured: face the bearing
    set_dial(panel, bearing, TURN_COLOR);
    std::snprintf(value, sizeof value, "%03u\xC2\xB0", static_cast<unsigned>(bearing));
    lv_label_set_text(panel->dial_value, value);
    lv_label_set_text_static(panel->dial_caption, "FROM NORTH");
    std::snprintf(line, sizeof line, "FACE %03u\xC2\xB0 %s", static_cast<unsigned>(bearing),
                  guidance_compass(bearing));
    lv_label_set_text(panel->command, line);
    lv_label_set_text_static(panel->headings, "HDG ---   (walk a few metres to measure it)");
  }
  const ShortText to_next = formatDistance(fix.to_maneuver_mm / MM_PER_M);
  if (fix.maneuver == MANEUVER_ARRIVE) {
    lv_label_set_text_fmt(panel->next, "%s  IN %s",
                          trip.returning ? "BASE" : "SITE", to_next.data());
  } else {
    const int32_t next_turn = guidance_turn_deg(fix.heading_deg, fix.next_heading_deg);
    guidance_format_turn(value, sizeof value, next_turn);
    lv_label_set_text_fmt(panel->next, "NEXT  %s  TO %03u\xC2\xB0  IN %s", value,
                          static_cast<unsigned>(fix.next_heading_deg), to_next.data());
  }
  show_hazard(panel, fix);
}

void guidance_panel_show_rerouting(t_guidance_panel *panel) {
  guidance_panel_set_visible(panel, true);
  lv_obj_set_style_border_color(panel->banner, lv_color_hex(CAUTION_COLOR), LV_PART_MAIN);
  set_dial(panel, 0, CAUTION_COLOR);
  lv_label_set_text_static(panel->dial_value, LV_SYMBOL_REFRESH);
  lv_label_set_text_static(panel->dial_caption, "OFF ROUTE");
  lv_label_set_text_static(panel->command, "OFF ROUTE");
  lv_label_set_text_static(panel->headings, "Computing a new way from here...");
  lv_label_set_text_static(panel->next, "");
  widgets::setVisible(panel->hazard, false);
}

void guidance_panel_show_blocked(t_guidance_panel *panel, const char *message) {
  guidance_panel_set_visible(panel, true);
  lv_obj_set_style_border_color(panel->banner, lv_color_hex(DANGER_COLOR), LV_PART_MAIN);
  set_dial(panel, 0, DANGER_COLOR);
  lv_label_set_text_static(panel->dial_value, LV_SYMBOL_WARNING);
  lv_label_set_text_static(panel->dial_caption, "NO WAY");
  lv_label_set_text_static(panel->command, "NO SAFE WAY");
  lv_label_set_text_static(panel->headings, message);
  lv_label_set_text_static(panel->next, "");
  lv_label_set_text_static(panel->end_label, "End");
  widgets::setVisible(panel->hazard, false);
}

static_assert(USV_PER_MSV / USV_PER_CENTI_MSV == 100, "two decimals of mSv");
static_assert(ON_COURSE_DEG < HALF_TURN_DEG);

} // namespace mars::ui
