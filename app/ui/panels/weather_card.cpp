#include "./weather_card.hpp"

#include "../../data/config.hpp"
#include "../Format.hpp"
#include "../PositionText.hpp"
#include "../Theme.hpp"
#include "../Widgets.hpp"
#include "../mission_time.hpp"
#include "../../guidance/guidance.hpp"

namespace mars::ui {

// Constants //
constexpr int32_t CARD_WIDTH = 380;
constexpr int32_t CARD_PADDING = 18;
constexpr int32_t BACK_WIDTH = 84;
constexpr int32_t BACK_HEIGHT = 36;
// Extends the small "Back" pill to a full-size touch target
constexpr int32_t BACK_TOUCH_SLOP = (theme::kTouchTarget - BACK_HEIGHT) / 2;
constexpr uint32_t CMS_PER_MS = 100;
constexpr uint32_t CMS_PER_DECI_MS = 10;
constexpr uint32_t DPA_PER_PA = 10;
// Dust opacity tau = dust_x40 / 40, shown with two decimals
constexpr uint32_t DUST_SCALE = 40;
constexpr uint32_t HUNDREDTHS = 100;
constexpr uint32_t USV_PER_MSV = 1000;
constexpr uint32_t USV_PER_CENTI_MSV = 10;

static void on_back(lv_event_t *event) {
  auto *const card = static_cast<t_weather_card *>(lv_event_get_user_data(event));
  card->on_close(card->context);
}

static lv_obj_t *make_line(lv_obj_t *parent, const lv_style_t *style) {
  lv_obj_t *const label = widgets::label(parent, "");
  lv_obj_set_width(label, lv_pct(100));
  if (style)
    lv_obj_add_style(label, style, LV_PART_MAIN);
  return (label);
}

void weather_card_create(t_weather_card *card, lv_obj_t *parent,
                         t_card_cb on_close, void *context) {
  const theme::Styles &styles = theme::styles();
  card->on_close = on_close;
  card->context = context;
  card->root = widgets::box(parent, &styles.card);
  lv_obj_set_size(card->root, CARD_WIDTH, LV_SIZE_CONTENT);
  lv_obj_set_style_max_height(card->root, lv_pct(96), LV_PART_MAIN);
  lv_obj_align(card->root, LV_ALIGN_TOP_LEFT, theme::kScreenMargin,
               theme::kScreenMargin / 2);
  lv_obj_set_style_pad_all(card->root, CARD_PADDING, LV_PART_MAIN);
  lv_obj_set_flex_flow(card->root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(card->root, theme::kGap / 2, LV_PART_MAIN);
  lv_obj_set_clickable(card->root, true); // swallows presses

  lv_obj_t *const back = widgets::button(card->root, &styles.backButton, on_back,
                                         card, &styles.backPressed);
  lv_obj_set_size(back, BACK_WIDTH, BACK_HEIGHT);
  lv_obj_set_ext_click_area(back, BACK_TOUCH_SLOP);
  lv_obj_add_style(back, &styles.smallText, LV_PART_MAIN);
  lv_obj_center(widgets::label(back, "Back"));

  card->title = make_line(card->root, nullptr);
  card->now = make_line(card->root, &styles.hint);
  card->ground = make_line(card->root, &styles.smallText);
  card->air = make_line(card->root, nullptr);
  card->wind = make_line(card->root, nullptr);
  card->radiation = make_line(card->root, nullptr);
  card->risk = make_line(card->root, &styles.smallText);
  lv_obj_t *const heading = make_line(card->root, &styles.hint);
  lv_label_set_text_static(heading, "Next sol");
  for (lv_obj_t *&row : card->forecast)
    row = make_line(card->root, &styles.smallText);
  weather_card_hide(card);
}

static void show_now(t_weather_card *card, const EnvironmentSample &s,
                     t_position position, uint32_t time_s, uint32_t now_s) {
  const ShortText lmst = format_lmst(position, time_s);
  if (time_s > now_s) {
    const ShortText in = formatDuration(time_s - now_s);
    lv_label_set_text_fmt(card->now, "At %s, in %s", lmst.data(), in.data());
  } else {
    lv_label_set_text_fmt(card->now, "Now, %s", lmst.data());
  }
  const PositionText where = formatPosition(position);
  if (s.hasTerrain)
    lv_label_set_text_fmt(card->ground, "%s\nGround %d m, slope %u.%u\xC2\xB0",
                          where.data(), static_cast<int>(s.elevationM),
                          static_cast<unsigned>(s.slopeHalfDegrees / 2),
                          static_cast<unsigned>(s.slopeHalfDegrees % 2 * 5));
  else
    lv_label_set_text_fmt(card->ground, "%s\nGround unknown", where.data());
  if (s.hasWeather) {
    const ShortText air = format_temperature(s.temperatureDeciKelvin);
    const uint32_t dust = uint32_t{s.dustOpacityX40} * HUNDREDTHS / DUST_SCALE;
    lv_label_set_text_fmt(card->air, "Air %s  \xE2\x80\xA2  %u Pa  \xE2\x80\xA2  dust %u.%02u", air.data(),
                          static_cast<unsigned>(s.pressureDeciPascal / DPA_PER_PA),
                          static_cast<unsigned>(dust / HUNDREDTHS),
                          static_cast<unsigned>(dust % HUNDREDTHS));
    const unsigned from_deg = (s.windFromX256 * 360U + 128U) / 256U % 360U;
    lv_label_set_text_fmt(
        card->wind, "Wind %u.%u m/s from %s (%03u\xC2\xB0)",
        static_cast<unsigned>(s.windCmPerSecond / CMS_PER_MS),
        static_cast<unsigned>(s.windCmPerSecond % CMS_PER_MS / CMS_PER_DECI_MS),
        guidance_compass(static_cast<uint16_t>(from_deg)), from_deg);
  } else {
    lv_label_set_text_static(card->air, "No weather data for this place.");
    lv_label_set_text_static(card->wind, "");
  }
  if (s.hasRadiation)
    lv_label_set_text_fmt(card->radiation, "Radiation %u.%02u mSv/sol",
                          static_cast<unsigned>(s.doseMicroSievertPerSol / USV_PER_MSV),
                          static_cast<unsigned>(s.doseMicroSievertPerSol % USV_PER_MSV /
                                                USV_PER_CENTI_MSV));
  else
    lv_label_set_text_static(card->radiation, "Radiation unknown here.");
  const LongText why = formatHazardReasons(s.reasons);
  lv_label_set_text_fmt(card->risk, "Risk: %s%s%s", hazardLevelName(s.level),
                        why[0] ? " - " : "", why.data());
}

void weather_card_show(t_weather_card *card, const IEnvironment &environment,
                       const char *title, t_position position, uint32_t time_s,
                       uint32_t now_s) {
  lv_label_set_text(card->title, title);
  show_now(card, environment.sample(position, time_s), position, time_s, now_s);
  const uint32_t step_s = WEATHER_CARD_FORECAST_STEP_HOURS * MARS_HOUR_S;
  for (uint32_t i = 0; i < WEATHER_CARD_FORECAST_ROWS; i++) {
    const uint32_t at_s = time_s + (i + 1) * step_s;
    const EnvironmentSample s = environment.sample(position, at_s);
    const ShortText lmst = format_lmst(position, at_s);
    if (!s.hasWeather) {
      lv_label_set_text_fmt(card->forecast[i], "%s   no data   %s", lmst.data(),
                            hazardLevelName(s.level));
      continue;
    }
    const ShortText air = format_temperature(s.temperatureDeciKelvin);
    lv_label_set_text_fmt(card->forecast[i], "%s   %s   wind %u m/s   %s",
                          lmst.data(), air.data(),
                          static_cast<unsigned>(s.windCmPerSecond / CMS_PER_MS),
                          hazardLevelName(s.level));
  }
  widgets::setVisible(card->root, true);
}

bool weather_card_is_visible(const t_weather_card *card) {
  return (!lv_obj_is_hidden(card->root));
}

void weather_card_hide(t_weather_card *card) {
  widgets::setVisible(card->root, false);
}

static_assert(WEATHER_CARD_FORECAST_ROWS * WEATHER_CARD_FORECAST_STEP_HOURS ==
                  MARS_HOURS_PER_SOL,
              "the forecast covers one sol");
static_assert(CMS_PER_MS / CMS_PER_DECI_MS == 10, "one decimal of m/s");

} // namespace mars::ui
