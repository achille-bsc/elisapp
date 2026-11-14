#include "./gathering.hpp"

t_data_point data_gathering(t_coord coord, e_data_type data_type) {
  t_data_point point = {};

  switch (data_type) {
  case WEATHER:
  case TEMPERATURE: // temperature is part of the weather packet
    point.weather = get_weather(coord);
    break;
  case LANDFORM:
    point.landform = get_landform(coord);
    break;
  case RADIATION:
    // TODO: no data source yet
    break;
  }
  return (point);
}
