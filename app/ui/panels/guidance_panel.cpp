#include "./guidance_panel.hpp"

#include "../../data/data.hpp"
#include "../Format.hpp"
#include "../Theme.hpp"
#include "../Widgets.hpp"
#include "../mission_time.hpp"

namespace mars::ui {

// Constants //
constexpr uint32_t BANNER_COLOR = 0x1B5E3F;   // navigator green
constexpr uint32_t CAUTION_COLOR = 0xE3A21A;
constexpr uint32_t DANGER_COLOR = 0xC62828;
constexpr uint32_t BANNER_TEXT = 0xFFFFFF;
constexpr int32_t BANNER_WIDTH = 640;
constexpr int32_t BANNER_PADDING = 18;
constexpr int32_t ARROW_SIZE = 72;
constexpr int32_t FOOTER_PADDING = 14;
constexpr int32_t BUTTON_WIDTH = 140;
constexpr uint32_t MM_PER_M = 1000;
constexpr uint32_t USV_PER_MSV = 1000;
// The dose is shown with two decimals of mSv
constexpr uint32_t USV_PER_CENTI_MSV = 10;

static void on_end(lv_event_t *event) {
  auto *const panel = static_cast<t_guidance_panel *>(lv_event_get_user_data(event));
  panel->on_end(panel->context);
}

static void on_recenter(lv_event_t *event) {
  auto *const panel = static_cast<t_guidance_panel *>(lv_event_get_user_data(event));
  panel->on_recenter(panel->context);
}

static const char *arrow_of(uint8_t maneuver) {
  switch (maneuver) {
  case MANEUVER_SLIGHT_LEFT:
  case MANEUVER_LEFT:
  case MANEUVER_SHARP_LEFT:
    return (LV_SYMBOL_LEFT);
  case MANEUVER_SLIGHT_RIGHT:
  case MANEUVER_RIGHT:
  case MANEUVER_SHARP_RIGHT:
    return (LV_SYMBOL_RIGHT);
  case MANEUVER_U_TURN:
    return (LV_SYMBOL_REFRESH);
  case MANEUVER_ARRIVE:
    return (LV_SYMBOL_HOME);
  default:
    return (LV_SYMBOL_UP);
  }
}

static const char *verb_of(uint8_t maneuver) {
  switch (maneuver) {
  case MANEUVER_SLIGHT_LEFT:
    return ("Bear left");
  case MANEUVER_SLIGHT_RIGHT:
    return ("Bear right");
  case MANEUVER_LEFT:
    return ("Turn left");
  case MANEUVER_RIGHT:
    return ("Turn right");
  case MANEUVER_SHARP_LEFT:
    return ("Sharp left");
  case MANEUVER_SHARP_RIGHT:
    return ("Sharp right");
  case MANEUVER_U_TURN:
    return ("Turn back");
  default:
    return ("Continue");
  }
}

static void set_banner_color(t_guidance_panel *panel, uint32_t color) {
  lv_obj_set_style_bg_color(panel->banner, lv_color_hex(color), LV_PART_MAIN);
}

static lv_obj_t *make_button(lv_obj_t *parent, const lv_style_t *style,
                             const lv_style_t *pressed, lv_event_cb_t on_click,
                             void *context, const char *text, lv_obj_t **label) {
  lv_obj_t *const button =
      widgets::button(parent, style, on_click, context, pressed);
  lv_obj_set_size(button, BUTTON_WIDTH, theme::kTouchTarget);
  lv_obj_t *const text_label = widgets::label(button, text);
  lv_obj_center(text_label);
  if (label)
    *label = text_label;
  return (button);
}

void guidance_panel_create(t_guidance_panel *panel, lv_obj_t *parent,
                           t_panel_cb on_end_cb, t_panel_cb on_recenter_cb,
                           void *context) {
  const theme::Styles &styles = theme::styles();
  panel->on_end = on_end_cb;
  panel->on_recenter = on_recenter_cb;
  panel->context = context;

  // Banner: [arrow] [distance / instruction / hazard]
  panel->banner = widgets::box(parent, &styles.card);
  lv_obj_set_size(panel->banner, BANNER_WIDTH, LV_SIZE_CONTENT);
  lv_obj_align(panel->banner, LV_ALIGN_TOP_MID, 0, theme::kScreenMargin / 2);
  lv_obj_set_style_pad_all(panel->banner, BANNER_PADDING, LV_PART_MAIN);
  lv_obj_set_style_text_color(panel->banner, lv_color_hex(BANNER_TEXT), LV_PART_MAIN);
  widgets::flex(panel->banner, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, theme::kGap);
  lv_obj_set_clickable(panel->banner, true); // swallows presses

  panel->arrow = widgets::label(panel->banner, LV_SYMBOL_UP);
  lv_obj_set_width(panel->arrow, ARROW_SIZE);
  lv_obj_set_style_text_font(panel->arrow, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_align(panel->arrow, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  lv_obj_t *const texts = widgets::box(panel->banner);
  lv_obj_set_flex_grow(texts, 1);
  lv_obj_set_height(texts, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(texts, theme::kGap / 2, LV_PART_MAIN);
  panel->distance = widgets::label(texts, "");
  lv_obj_set_style_text_font(panel->distance, &lv_font_montserrat_24, LV_PART_MAIN);
  panel->instruction = widgets::label(texts, "");
  lv_obj_set_width(panel->instruction, lv_pct(100));
  panel->hazard = widgets::label(texts, "");
  lv_obj_set_width(panel->hazard, lv_pct(100));
  lv_obj_set_style_bg_opa(panel->hazard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(panel->hazard, theme::kGap / 2, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel->hazard, theme::kGap / 2, LV_PART_MAIN);
  lv_obj_add_style(panel->hazard, &styles.smallText, LV_PART_MAIN);

  // Bottom bar: [summary / arrival] [Re-centre] [End]
  panel->footer = widgets::box(parent, &styles.card);
  lv_obj_set_size(panel->footer, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_align(panel->footer, LV_ALIGN_BOTTOM_MID, 0, theme::kCardRadius);
  lv_obj_set_style_pad_all(panel->footer, FOOTER_PADDING, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(panel->footer, FOOTER_PADDING + theme::kCardRadius,
                              LV_PART_MAIN);
  lv_obj_set_style_pad_hor(panel->footer, theme::kScreenMargin, LV_PART_MAIN);
  widgets::flex(panel->footer, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, theme::kGap);
  lv_obj_set_clickable(panel->footer, true);

  lv_obj_t *const info = widgets::box(panel->footer);
  lv_obj_set_flex_grow(info, 1);
  lv_obj_set_height(info, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  panel->summary = widgets::label(info, "");
  lv_obj_set_style_text_font(panel->summary, &lv_font_montserrat_24, LV_PART_MAIN);
  panel->arrival = widgets::label(info, "");
  lv_obj_add_style(panel->arrival, &styles.hint, LV_PART_MAIN);

  panel->recenter = make_button(panel->footer, &styles.pill, nullptr, on_recenter,
                                panel, LV_SYMBOL_GPS " Re-centre", nullptr);
  make_button(panel->footer, &styles.backButton, &styles.backPressed, on_end,
              panel, "End", &panel->end_label);

  set_banner_color(panel, BANNER_COLOR);
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
    lv_label_set_text_fmt(panel->hazard, "%s here: %s",
                          danger ? "Danger" : "Caution", why.data());
  else
    lv_label_set_text_fmt(panel->hazard, "%s in %s: %s",
                          danger ? "Danger" : "Caution", where.data(), why.data());
  widgets::setVisible(panel->hazard, true);
}

void guidance_panel_update(t_guidance_panel *panel, const t_guidance_fix &fix,
                           const t_guidance_trip &trip) {
  guidance_panel_set_visible(panel, true);
  const ShortText left_time = formatDuration(fix.remaining_s);
  const ShortText left_distance = formatDistance(fix.remaining_mm / MM_PER_M);
  const ShortText arrival =
      format_lmst(trip.destination, trip.time_s + fix.remaining_s);
  lv_label_set_text_fmt(panel->summary, "%s  \xE2\x80\xA2  %s", left_time.data(),
                        left_distance.data());
  lv_label_set_text_fmt(panel->arrival, "Arrival %s  \xE2\x80\xA2  dose %u.%02u mSv",
                        arrival.data(),
                        static_cast<unsigned>(trip.dose_left_usv / USV_PER_MSV),
                        static_cast<unsigned>(trip.dose_left_usv % USV_PER_MSV /
                                              USV_PER_CENTI_MSV));
  lv_label_set_text_static(panel->end_label, "End");

  if (fix.state == GUIDANCE_ARRIVED) {
    set_banner_color(panel, BANNER_COLOR);
    lv_label_set_text_static(panel->arrow, LV_SYMBOL_HOME);
    lv_label_set_text_static(panel->distance, "You have arrived");
    lv_label_set_text_static(panel->instruction, "The destination is here.");
    lv_label_set_text_static(panel->end_label, "Done");
    show_hazard(panel, fix);
    return;
  }
  if (fix.state == GUIDANCE_OFF_ROUTE) {
    guidance_panel_show_rerouting(panel);
    return;
  }
  set_banner_color(panel, BANNER_COLOR);
  lv_label_set_text_static(panel->arrow, arrow_of(fix.maneuver));
  const ShortText to_next = formatDistance(fix.to_maneuver_mm / MM_PER_M);
  lv_label_set_text(panel->distance, to_next.data());
  if (fix.maneuver == MANEUVER_ARRIVE)
    lv_label_set_text_fmt(panel->instruction, "Head %s to the destination",
                          guidance_compass(fix.heading_deg));
  else
    lv_label_set_text_fmt(panel->instruction, "%s, then head %s",
                          verb_of(fix.maneuver),
                          guidance_compass(fix.next_heading_deg));
  show_hazard(panel, fix);
}

void guidance_panel_show_rerouting(t_guidance_panel *panel) {
  guidance_panel_set_visible(panel, true);
  set_banner_color(panel, CAUTION_COLOR);
  lv_label_set_text_static(panel->arrow, LV_SYMBOL_REFRESH);
  lv_label_set_text_static(panel->distance, "Off route");
  lv_label_set_text_static(panel->instruction, "Finding a new way from here...");
  widgets::setVisible(panel->hazard, false);
}

void guidance_panel_show_blocked(t_guidance_panel *panel, const char *message) {
  guidance_panel_set_visible(panel, true);
  set_banner_color(panel, DANGER_COLOR);
  lv_label_set_text_static(panel->arrow, LV_SYMBOL_WARNING);
  lv_label_set_text_static(panel->distance, "No safe way");
  lv_label_set_text_static(panel->instruction, message);
  widgets::setVisible(panel->hazard, false);
}

static_assert(USV_PER_MSV / USV_PER_CENTI_MSV == 100, "two decimals of mSv");

} // namespace mars::ui
