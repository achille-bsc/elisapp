#pragma once

#include <lvgl.h>

#include <cstdint>

#include "../../core/environment/IEnvironment.hpp"
#include "../../core/gps/Position.hpp"

// Weather report of a place: the conditions now (local air temperature and
// pressure, wind, dust, radiation dose rate, risk and its reasons) and the
// forecast of the next sol from the diurnal model, every few Mars hours.

namespace mars::ui {

// Constants //
constexpr uint8_t WEATHER_CARD_FORECAST_ROWS = 6;
constexpr uint32_t WEATHER_CARD_FORECAST_STEP_HOURS = 4; // 6 x 4 h = 1 sol

// Structures //
typedef void (*t_card_cb)(void *context);

struct s_weather_card {
  lv_obj_t *root;
  lv_obj_t *title;
  lv_obj_t *now;
  lv_obj_t *air;
  lv_obj_t *wind;
  lv_obj_t *radiation;
  lv_obj_t *risk;
  lv_obj_t *forecast[WEATHER_CARD_FORECAST_ROWS];
  t_card_cb on_close;
  void *context;
} typedef t_weather_card;

// Functions //
void weather_card_create(t_weather_card *card, lv_obj_t *parent,
                         t_card_cb on_close, void *context);
// `place` is copied. `environment` is read now only.
void weather_card_show(t_weather_card *card, const IEnvironment &environment,
                       const char *place, t_position position, uint32_t time_s);
void weather_card_hide(t_weather_card *card);

} // namespace mars::ui
