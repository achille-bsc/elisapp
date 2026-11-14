#pragma once

#include "./coord.hpp"
#include "./landform.hpp"
#include "./weather.hpp"

// enumarations //
enum e_data_type { WEATHER, LANDFORM, TEMPERATURE, RADIATION };

// Structures //
// Only the field matching the requested e_data_type is filled, the others
// stay zeroed with valid == false.
struct s_data_point {
  t_weather weather;
  t_landform landform;
} typedef t_data_point;

// Functions //
t_data_point data_gathering(t_coord, e_data_type);
