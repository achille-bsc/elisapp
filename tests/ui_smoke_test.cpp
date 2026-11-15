#include <lvgl.h>

#include "core/gps/packet/MarsPositionPacket.hpp"
#include "core/mission/JsonMissionRepository.hpp"
#include "core/poi/JsonPointOfInterestRepository.hpp"
#include "data/data.hpp"
#include "data/sim/weather_sim.hpp"
#include "integration/DataEnvironment.hpp"
#include "integration/MarspathRoutePlanner.hpp"
#include "integration/Tuning.hpp"
#include "platform/gps_sim.hpp"
#include "test_zone.hpp"
#include "ui/MainScreen.hpp"
#include "ui/map/data_background.hpp"
#include <cstdio>
#include <vector>

// The whole screen on a display without a window: real missions and points
// of interest, the team's planner, the data module, the simulated GPS. It
// draws every layer, opens a mission, starts the guidance and walks to the
// end. Run: make test-ui

static int g_failed = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_checks++;                                                                \
    if (!(cond)) {                                                             \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
      g_failed++;                                                              \
    }                                                                          \
  } while (0)

// Constants //
constexpr int32_t SCREEN_WIDTH = 1133;
constexpr int32_t SCREEN_HEIGHT = 744;
// Partial rendering through a buffer of a tenth of the screen
constexpr uint32_t BUFFER_LINES = SCREEN_HEIGHT / 10;
constexpr uint32_t RGB565_BYTES = 2;
// Relief around the base and the missions (Jezero), ~53 m cells: fine terrain
constexpr uint32_t ZONE_STEP_UDEG = 900;
constexpr uint32_t ZONE_WEST_UDEG = 77000000;
constexpr int32_t ZONE_NORTH_UDEG = 18700000;
constexpr uint16_t ZONE_WIDTH = 800;
constexpr uint16_t ZONE_HEIGHT = 500;
constexpr int16_t ZONE_ELEVATION_M = -2550;
constexpr uint32_t FIX_PERIOD_S = 30;
constexpr uint32_t MAX_FIXES = 2000;
constexpr uint32_t LV_HEAP_MAX_USED_PERCENT = 90;

// Mission time set by the test
class test_clock final : public mars::IMissionClock {
public:
  [[nodiscard]] uint32_t nowSeconds() const noexcept override { return (now_s); }
  uint32_t now_s = 0;
};

class sim_navigation final : public mars::ui::INavigationListener {
public:
  explicit sim_navigation(const test_clock &clock) : clock_{clock} {}
  void onGuidanceStarted(std::span<const mars::t_position> route, uint32_t departure_s) override {
    gps_sim_follow(route.data(), static_cast<uint32_t>(route.size()), WALK_SPEED_MM_S, departure_s);
    started++;
  }
  void onGuidanceEnded() override {
    gps_sim_stop(clock_.nowSeconds());
    ended++;
  }
  uint32_t started = 0;
  uint32_t ended = 0;

private:
  const test_clock &clock_;
};

// The screen as flushed, to check what is really drawn
static std::vector<uint16_t> g_frame(size_t{SCREEN_WIDTH} * SCREEN_HEIGHT);

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
  const uint16_t *from = reinterpret_cast<const uint16_t *>(pixels);
  for (int32_t y = area->y1; y <= area->y2; y++)
    for (int32_t x = area->x1; x <= area->x2; x++)
      g_frame[size_t(y) * SCREEN_WIDTH + size_t(x)] = *from++;
  lv_display_flush_ready(display);
}

// "No data" colour of the map picture (0x1E1410 in RGB565): only the data
// background paints it, so seeing it proves the picture is on screen
constexpr uint16_t NO_DATA_RGB565 = 0x18A2;
// Around the small test zone most of the first view has no data
constexpr size_t MIN_NO_DATA_PERCENT = 50;

static size_t no_data_percent() {
  size_t count = 0;
  for (const uint16_t pixel : g_frame)
    count += pixel == NO_DATA_RGB565;
  return (count * 100 / g_frame.size());
}

static bool near_m(mars::t_position a, mars::t_position b, double metres) {
  return (test_distance_m(a, b) <= metres);
}

static uint32_t g_tick_ms = 0;
static uint32_t tick() { return (g_tick_ms); }

static void render() {
  g_tick_ms += LV_DEF_REFR_PERIOD;
  lv_timer_handler();
  lv_refr_now(nullptr);
}

int main() {
  lv_init();
  lv_tick_set_cb(tick);
  lv_display_t *const display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  std::vector<uint8_t> buffer(size_t{SCREEN_WIDTH} * BUFFER_LINES * RGB565_BYTES);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, buffer.data(), nullptr, static_cast<uint32_t>(buffer.size()),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);

  const t_landform_header header = {LANDFORM_MAGIC, ZONE_WEST_UDEG, ZONE_NORTH_UDEG, ZONE_STEP_UDEG,
                                    ZONE_WIDTH,     ZONE_HEIGHT,    LANDFORM_VERSION, 0};
  uint32_t size = 0;
  std::vector<uint32_t> zone = test_zone_make(header, ZONE_ELEVATION_M, nullptr, &size);
  CHECK(landform_init(zone.data(), size));
  std::vector<uint8_t> cache(weather_cache_bytes(mars::tuning::kWeatherCacheSlots));
  weather_set_cache(cache.data(), static_cast<uint32_t>(cache.size()));
  weather_set_link(weather_sat_sim);

  mars::JsonPointOfInterestRepository points;
  CHECK(points.load(mars::tuning::kPointsOfInterestPath));
  mars::JsonMissionRepository missions;
  CHECK(missions.load(mars::tuning::kMissionsPath));
  CHECK(!missions.all().empty());
  mars::MarspathRoutePlanner routes;
  mars::DataEnvironment environment;
  test_clock clock;
  mars::ui::data_background backgrounds{clock};
  sim_navigation navigation{clock};
  const mars::t_position base = {mars::tuning::kBaseLatitudeE7, mars::tuning::kBaseLongitudeE7};
  gps_sim_init(base);

  mars::ui::MainScreen screen{
      lv_screen_active(), {points, missions, routes, backgrounds, environment, clock, navigation}, base};
  render();
  CHECK(no_data_percent() >= MIN_NO_DATA_PERCENT);
  // Every layer is computed and drawn
  screen.showLayer(mars::ui::MapLayer::Weather);
  render();
  screen.showLayer(mars::ui::MapLayer::Risk);
  render();
  CHECK(no_data_percent() >= MIN_NO_DATA_PERCENT);
  screen.showLayer(mars::ui::MapLayer::Terrain);
  render();

  CHECK(!screen.openMission(missions.all().size()));
  CHECK(!screen.startGuidance()); // nothing open yet
  CHECK(screen.openMission(0));
  render();
  CHECK(screen.startGuidance());
  CHECK(screen.isGuiding() && navigation.started == 1);
  render();

  // Stray 300 m off the route: after a few fixes a new route is planned from
  // there, the receiver follows it and the guidance is back on route
  constexpr double STRAY_EAST_M = 300;
  const mars::t_position stray = test_offset(gps_sim_position(clock.now_s), STRAY_EAST_M, 0);
  constexpr uint32_t STRAY_FIXES = GUIDANCE_OFF_ROUTE_FIXES + 1;
  // Each stray fix is past the 30 s cool-down between two plans
  constexpr uint32_t STRAY_PERIOD_S = 31;
  for (uint32_t i = 0; i < STRAY_FIXES; i++) {
    clock.now_s += STRAY_PERIOD_S;
    screen.setCurrentPosition(stray);
    render();
  }
  CHECK(navigation.started == 2);
  CHECK(screen.lastFix().state == GUIDANCE_ON_ROUTE);
  // The receiver restarted from there: at most one fix period of walking away
  constexpr double MM_PER_M = 1000.0;
  CHECK(near_m(gps_sim_position(clock.now_s), stray,
               STRAY_PERIOD_S * WALK_SPEED_MM_S / MM_PER_M + 1));

  // Walk: one GPS packet per fix, decoded as from a real receiver
  uint32_t fixes = 0;
  while (fixes < MAX_FIXES && screen.lastFix().state != GUIDANCE_ARRIVED) {
    clock.now_s += FIX_PERIOD_S;
    const std::optional<mars::t_position> p =
        mars::PositionPacket::decode(gps_sim_packet(clock.now_s));
    CHECK(p.has_value());
    screen.setCurrentPosition(*p);
    CHECK(screen.lastFix().state != GUIDANCE_OFF_ROUTE);
    render();
    fixes++;
  }
  CHECK(screen.lastFix().state == GUIDANCE_ARRIVED);
  CHECK(navigation.ended == 1);
  // The arrival stays as it was reached
  const t_guidance_fix arrived = screen.lastFix();
  clock.now_s += FIX_PERIOD_S;
  screen.setCurrentPosition(*mars::PositionPacket::decode(gps_sim_packet(clock.now_s)));
  CHECK(screen.lastFix().state == GUIDANCE_ARRIVED && screen.lastFix().remaining_s == arrived.remaining_s);
  printf("ui: arrived after %u fixes (%u s of mission time)\n", static_cast<unsigned>(fixes),
         static_cast<unsigned>(clock.now_s));

  lv_mem_monitor_t memory;
  lv_mem_monitor(&memory);
  printf("ui: LVGL heap %u %% used, %u %% fragmented\n", static_cast<unsigned>(memory.used_pct),
         static_cast<unsigned>(memory.frag_pct));
  CHECK(memory.used_pct < LV_HEAP_MAX_USED_PERCENT);

  if (g_failed)
    printf("%d / %d check(s) FAILED\n", g_failed, g_checks);
  else
    printf("all %d ui checks passed\n", g_checks);
  weather_set_cache(nullptr, 0);
  landform_clear();
  return (g_failed != 0);
}

static_assert(ZONE_STEP_UDEG <= HAZARD_FINE_TERRAIN_MAX_STEP_UDEG, "fine layer");
static_assert(uint64_t{ZONE_WIDTH} * ZONE_STEP_UDEG > 500000, "covers the missions");
