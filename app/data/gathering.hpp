#pragma once

#include "./core/coord.hpp"
#include "./landform/landform.hpp"
#include "./radiation/radiation.hpp"
#include "./weather/weather_model.hpp"
#include <cstdint>

// Single entry point of the data module for the front and the pathfinding.
//
// marspath bridge (app/core/pathfinding), GeoPoint is in E7:
//   marspath::TerrainAdapter terrain([](marspath::GeoPoint p) {
//     return static_cast<float>(
//         landform_get(coord_from_e7(p.lat_e7, p.lon_e7)).elevation_m);
//   });
//   marspath::RiskMapAdapter risk([time_s](marspath::GeoPoint p) {
//     return data_is_hazard(coord_from_e7(p.lat_e7, p.lon_e7), time_s);
//   });

// Enumerations //
enum e_data_type { DATA_LANDFORM, DATA_WEATHER, DATA_RADIATION, DATA_ALL };

// Structures //
// Only the fields matching the requested e_data_type are filled, the others
// stay zeroed with valid == false. The weather is the local estimate.
struct s_data_point {
  t_landform landform;
  t_weather weather;
  t_radiation radiation;
} typedef t_data_point;

// Functions //
t_data_point data_gathering(t_coord coord, uint32_t time_s, e_data_type type);
// True where a human must not walk at that time: hazard_assess() level is
// HAZARD_DANGER (hazard/hazard.hpp gives the reasons and the hazard map).
// Missing weather (no satellite link) is not a hazard by itself.
bool data_is_hazard(t_coord coord, uint32_t time_s);
