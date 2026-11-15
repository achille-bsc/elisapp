#pragma once

#include <lvgl.h>

#include <cstdint>
#include <vector>

#include "../../core/time/IMissionClock.hpp"
#include "./IMapBackgroundProvider.hpp"

// Map pictures computed from the data module for the region on screen:
//   Terrain : elevation colour ramp, shaded relief lit from the north-west
//   Weather : local air temperature at the current mission time (diurnal
//             model), over the same shading
//   Risk    : hazard level of each place at the current mission time
// One sample every DATA_BACKGROUND_STEP_PX screen pixels, stretched by LVGL.

namespace mars::ui {

// Constants //
// 4 x 4 pixels per sample: 53 k samples for the iPad mini screen
constexpr int32_t DATA_BACKGROUND_STEP_PX = 4;
constexpr uint8_t DATA_BACKGROUND_FRAMES = 2;

// Structures //
struct s_background_frame {
  std::vector<uint32_t> words; // 4 bytes aligned for LVGL, RGB565 rows
  lv_draw_buf_t image;
} typedef t_background_frame;

class data_background final : public IMapBackgroundProvider {
public:
  // `clock` must outlive the provider; a DataSession must be alive
  explicit data_background(const IMissionClock &clock);

  [[nodiscard]] const void *background(MapLayer layer,
                                       const MapProjection &projection) override;

private:
  const IMissionClock &clock_;
  // Two pictures in turn: the one on screen stays valid while the next one
  // is computed, as IMapBackgroundProvider requires
  t_background_frame frames_[DATA_BACKGROUND_FRAMES];
  uint8_t next_ = 0;
  // Interpolated elevations, NaN where there is no relief: kept in float so
  // the shading does not turn 1 m rounding steps into stripes
  std::vector<float> elevations_;
};

} // namespace mars::ui
