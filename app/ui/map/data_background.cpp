#include "./data_background.hpp"

#include "./render/Canvas.hpp"

namespace mars::ui {

data_background::data_background(const IMissionClock &clock,
                                 const char *weatherSource)
    : clock_{clock}, weather_{weatherSource} {}

render::ILayerPainter &data_background::painter(MapLayer layer) {
  if (layer == MapLayer::Weather)
    return (weather_);
  if (layer == MapLayer::Risk)
    return (risk_);
  if (layer == MapLayer::Radiation)
    return (radiation_);
  return (terrain_);
}

LegendSpec data_background::legend(MapLayer layer) const {
  if (layer == MapLayer::Weather)
    return (weather_.legend());
  if (layer == MapLayer::Risk)
    return (risk_.legend());
  if (layer == MapLayer::Radiation)
    return (radiation_.legend());
  return (terrain_.legend());
}

const void *data_background::background(MapLayer layer,
                                        const MapProjection &projection) {
  samplesChanged_ = samples_.update(projection) || samplesChanged_;
  const uint32_t w = samples_.width();
  const uint32_t h = samples_.height();
  if (w == 0 || h == 0)
    return (nullptr);
  render::ILayerPainter &chosen = painter(layer);
  const uint32_t time_s = clock_.nowSeconds();
  if (shown_ && !samplesChanged_ && layer == shownLayer_ &&
      (!chosen.dependsOnTime() || time_s == shownTime_))
    return (shown_);

  t_background_frame &frame = frames_[next_];
  next_ = static_cast<uint8_t>((next_ + 1) % FRAMES);
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
  frame.words.resize((size_t{stride} * h + sizeof(uint32_t) - 1) /
                     sizeof(uint32_t));
  uint8_t *const bytes = reinterpret_cast<uint8_t *>(frame.words.data());
  render::Canvas canvas{bytes, w, h, stride};
  chosen.paint(samples_, time_s, canvas);

  if (lv_draw_buf_init(&frame.image, w, h, LV_COLOR_FORMAT_RGB565, stride,
                       frame.words.data(), stride * h) != LV_RESULT_OK) {
    shown_ = nullptr;
    return (nullptr);
  }
  // Same address, new pixels: LVGL must not reuse a cached copy
  lv_image_cache_drop(&frame.image);
  shown_ = &frame.image;
  shownLayer_ = layer;
  shownTime_ = time_s;
  samplesChanged_ = false;
  return (shown_);
}


} // namespace mars::ui
