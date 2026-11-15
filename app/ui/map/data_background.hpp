#pragma once

#include <lvgl.h>

#include <array>
#include <cstdint>
#include <vector>

#include "../../core/time/IMissionClock.hpp"
#include "./IMapBackgroundProvider.hpp"
#include "./render/Legend.hpp"
#include "./render/RadiationPainter.hpp"
#include "./render/RiskPainter.hpp"
#include "./render/TerrainPainter.hpp"
#include "./render/ViewSamples.hpp"
#include "./render/WeatherPainter.hpp"

// Map pictures from the data module, one sample per pixel. A picture is
// computed again only when the view, the layer or (time layers) the time
// changes.

namespace mars::ui {

// Structures //
struct s_background_frame {
  std::vector<uint32_t> words; // 4 bytes aligned for LVGL, RGB565 rows
  lv_draw_buf_t image;
} typedef t_background_frame;

class data_background final : public IMapBackgroundProvider,
                              public IMapLegendSource {
public:
  // `clock` and `weatherSource` must outlive the provider.
  data_background(const IMissionClock &clock, const char *weatherSource);

  [[nodiscard]] const void *background(MapLayer layer,
                                       const MapProjection &projection) override;
  [[nodiscard]] LegendSpec legend(MapLayer layer) const override;

private:
  // Two pictures in turn: the one on screen stays valid
  static constexpr uint8_t FRAMES = 2;
  static_assert(FRAMES >= 2, "the shown picture stays valid");

  [[nodiscard]] render::ILayerPainter &painter(MapLayer layer);

  const IMissionClock &clock_;
  render::ViewSamples samples_;
  render::TerrainPainter terrain_;
  render::WeatherPainter weather_;
  render::RiskPainter risk_;
  render::RadiationPainter radiation_;
  t_background_frame frames_[FRAMES];
  uint8_t next_ = 0;
  const void *shown_ = nullptr;
  MapLayer shownLayer_ = MapLayer::Terrain;
  uint32_t shownTime_ = 0;
  bool samplesChanged_ = true;
};

} // namespace mars::ui
