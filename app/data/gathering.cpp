#include "./gathering.hpp"
#include "./hazard/hazard.hpp"

t_data_point data_gathering(t_coord coord, uint32_t time_s, e_data_type type) {
  t_data_point point = {};
  if (type == DATA_LANDFORM || type == DATA_ALL)
    point.landform = landform_get(coord);
  if (type == DATA_WEATHER || type == DATA_ALL)
    point.weather = weather_estimate(coord, time_s);
  if (type == DATA_ALL) // reuses the weather above: one model run per point
    point.radiation = radiation_from(point.landform, point.weather);
  else if (type == DATA_RADIATION)
    point.radiation = radiation_estimate(coord, time_s);
  return (point);
}

bool data_is_hazard(t_coord coord, uint32_t time_s) {
  return (hazard_assess(coord, time_s).level == HAZARD_DANGER);
}

static_assert(sizeof(t_data_point) == 22, "6 + 10 + 6 bytes, no padding");
