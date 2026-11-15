#include <lvgl.h>

#include "core/gps/packet/MarsPositionPacket.hpp"
#include "core/mission/JsonMissionRepository.hpp"
#include "core/poi/JsonPointOfInterestRepository.hpp"
#include "core/site/JsonMissionSiteRepository.hpp"
#include "data/data.hpp"
#include "data/sim/weather_sim.hpp"
#include "integration/DataEnvironment.hpp"
#include "integration/DataStormForecast.hpp"
#include "integration/MarspathRoutePlanner.hpp"
#include "integration/Tuning.hpp"
#include "platform/gps_sim.hpp"
#include "test_zone.hpp"
#include "ui/MainScreen.hpp"
#include "ui/map/data_background.hpp"
#include <array>
#include <cstdio>
#include <cstring>
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

// A colour of the map picture, dithered to RGB565: within one RGB565 step
static bool near_color(uint16_t pixel, uint32_t hex) {
  constexpr int RED_STEP = 8;
  constexpr int GREEN_STEP = 4;
  const int r = (pixel >> 11) << 3;
  const int g = (pixel >> 5 & 0x3F) << 2;
  const int b = (pixel & 0x1F) << 3;
  const auto close = [](int a, uint32_t c, int step) {
    return (a - static_cast<int>(c) <= step && static_cast<int>(c) - a <= step);
  };
  return (close(r, hex >> 16 & 0xFF, RED_STEP) && close(g, hex >> 8 & 0xFF, GREEN_STEP) &&
          close(b, hex & 0xFF, RED_STEP));
}

static size_t percent_near(uint32_t hex) {
  size_t count = 0;
  for (const uint16_t pixel : g_frame)
    count += near_color(pixel, hex);
  return (count * 100 / g_frame.size());
}

// Flat test zone without image: the lowest colour of the relief ramp
constexpr uint32_t FLAT_GROUND = 0x2E1A12;
// The first view is the ground around the base, all inside the zone
constexpr size_t MIN_GROUND_PERCENT = 50;
// Zoomed out, most of the view is outside the zone: "no data"
constexpr uint32_t NO_DATA = 0x1E1410;
constexpr size_t MIN_NO_DATA_PERCENT = 30;
constexpr int32_t ZOOM_OUT_STEPS = -6;

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

// Navigation points: evenly spread, the true distance of the destination
// even when the slots run out, no point crowding the destination
static void test_spread_along_route() {
  // 101 vertices 0.0001 deg apart along the equator (~5.9 m each)
  constexpr int32_t STEP_E7 = 1000;
  std::vector<mars::t_position> route;
  for (uint32_t i = 0; i <= 100; i++)
    route.push_back({0, 100'000'000 + i * STEP_E7});
  uint64_t total = 0;
  for (size_t i = 1; i < route.size(); i++)
    total += coord_distance_mm(coord_from_e7_east(0, route[i - 1].longitudeEastE7),
                               coord_from_e7_east(0, route[i].longitudeEastE7));
  std::array<mars::ui::RouteMark, mars::ui::WaypointLayer::kMaxPoints> out{};
  size_t n = mars::ui::spreadAlongRoute(route, 8, out);
  CHECK(n >= 2 && n <= 9 && out[n - 1].vertex == 100);
  const uint64_t spacing = total / 8;
  for (size_t i = 0; i + 1 < n; i++) {
    CHECK(out[i].vertex < out[i + 1].vertex);
    CHECK(out[i].alongMm + spacing / 2 <= total); // not on the destination
  }
  // More points asked than slots: the destination keeps its true distance
  n = mars::ui::spreadAlongRoute(route, 50, out);
  CHECK(n == out.size() && out[n - 1].vertex == 100);
  CHECK(out[n - 1].alongMm == total);
  // Too short a route: nothing
  CHECK(mars::ui::spreadAlongRoute(std::span(route).first(1), 8, out) == 0);
}

// The mission site file: fields read, strings kept, bad files refused
static void test_mission_site() {
  const mars::MissionSite fallback = {"Fallback", "", {0, 0}, 0.0, 8, 3000};
  char good[] = R"({"mission_site": {"name": "Base", "latitude": -4.5, "longitude": -40,
    "solar_longitude_deg": 90, "start_local_hour": 13, "view_radius_km": 2,
    "zone_radius_km": 60}})";
  mars::MissionSite site = fallback;
  CHECK(mars::JsonMissionSiteRepository::parse(good, good + sizeof good - 1, site));
  CHECK(std::strcmp(site.name, "Base") == 0 && site.position.latitudeE7 == -45'000'000);
  CHECK(site.position.longitudeEastE7 == 3'200'000'000U && site.startLocalHour == 13);
  CHECK(site.viewRadiusM == 2000 && site.solarLongitudeDegrees == 90.0);
  char far_north[] = R"({"mission_site": {"name": "X", "latitude": 95, "longitude": 0}})";
  site = fallback;
  CHECK(!mars::JsonMissionSiteRepository::parse(far_north, far_north + sizeof far_north - 1, site));
  CHECK(site.name == fallback.name);
  char no_name[] = R"({"mission_site": {"latitude": 1, "longitude": 2}})";
  CHECK(!mars::JsonMissionSiteRepository::parse(no_name, no_name + sizeof no_name - 1, site));
  mars::JsonMissionSiteRepository missing{fallback};
  CHECK(!missing.load("no/such/site.json") && missing.site().name == fallback.name);
  mars::JsonMissionSiteRepository real{fallback};
  CHECK(real.load(mars::tuning::kMissionSitePath) && real.site().viewRadiusM > 0);
}

int main() {
  test_spread_along_route();
  test_mission_site();
  lv_init();
  lv_tick_set_cb(tick);
  lv_display_t *const display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  std::vector<uint8_t> buffer(size_t{SCREEN_WIDTH} * BUFFER_LINES * RGB565_BYTES);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, buffer.data(), nullptr, static_cast<uint32_t>(buffer.size()),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);

  const t_landform_header header = {LANDFORM_MAGIC, ZONE_WEST_UDEG, ZONE_NORTH_UDEG, ZONE_STEP_UDEG,
                                    ZONE_WIDTH,     ZONE_HEIGHT,    LANDFORM_VERSION, 0, 0};
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
  mars::DataStormForecast storms;
  // No photo in the test
  class no_photos final : public mars::ui::IPlacePhotos {
  public:
    const void *photo(const mars::PointOfInterest &) override { return (nullptr); }
  } photos;
  test_clock clock;
  mars::ui::data_background backgrounds{clock, "Simulated weather."};
  sim_navigation navigation{clock};
  const mars::t_position base = {mars::tuning::kBaseLatitudeE7, mars::tuning::kBaseLongitudeE7};
  const mars::MissionSite site = {"Test base", "", base, 0.0, 8, 3000};
  gps_sim_init(base);

  mars::ui::MainScreen screen{lv_screen_active(),
                              {points, missions, routes, backgrounds, backgrounds, site, environment, storms, photos, clock,
                               navigation},
                              base};
  render();
  CHECK(percent_near(FLAT_GROUND) >= MIN_GROUND_PERCENT);
  CHECK(backgrounds.legend(mars::ui::MapLayer::Terrain).hasGradient);
  // Every layer is computed and drawn, with its legend
  screen.showLayer(mars::ui::MapLayer::Weather);
  render();
  CHECK(backgrounds.legend(mars::ui::MapLayer::Weather).hasGradient);
  screen.showLayer(mars::ui::MapLayer::Risk);
  render();
  // Flat known ground, simulated weather: safe, the first legend entry
  const mars::ui::LegendSpec risk = backgrounds.legend(mars::ui::MapLayer::Risk);
  CHECK(risk.itemCount >= 1 && risk.items[0].color == 0x009E73);
  screen.showLayer(mars::ui::MapLayer::Terrain);
  render();
  screen.zoomBy(ZOOM_OUT_STEPS);
  render();
  CHECK(percent_near(NO_DATA) >= MIN_NO_DATA_PERCENT);

  CHECK(!screen.openMission(missions.all().size()));
  CHECK(!screen.startGuidance()); // nothing open yet
  CHECK(screen.openMission(0));
  render();
  // Navigation points along the route, the destination last, times rising
  const std::span<const mars::ui::NavigationPoint> marks = screen.navigationPoints();
  CHECK(!marks.empty() && marks.size() <= mars::ui::WaypointLayer::kMaxPoints);
  // The way out ends at the mission, the way back at the base
  size_t last_out = 0;
  for (size_t i = 0; i < marks.size(); i++)
    if (!marks[i].back)
      last_out = i;
  CHECK(!marks.empty() && near_m(marks[last_out].position, missions.all()[0].destination, 50));
  CHECK(!marks.empty() && marks.back().back && near_m(marks.back().position, base, 50));
  for (size_t i = 1; i < marks.size(); i++)
    CHECK(marks[i].etaSeconds >= marks[i - 1].etaSeconds && marks[i].number == i + 1);
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

  // On site, End is "Return to base": leg 2 is guided from here to the base
  screen.endGuidance();
  CHECK(screen.isGuiding() && screen.isReturning() && navigation.started == 3);
  CHECK(screen.lastFix().state != GUIDANCE_ARRIVED);
  render();
  fixes = 0;
  while (fixes < MAX_FIXES && screen.lastFix().state != GUIDANCE_ARRIVED) {
    clock.now_s += FIX_PERIOD_S;
    screen.setCurrentPosition(*mars::PositionPacket::decode(gps_sim_packet(clock.now_s)));
    CHECK(screen.lastFix().state != GUIDANCE_OFF_ROUTE);
    render();
    fixes++;
  }
  CHECK(screen.lastFix().state == GUIDANCE_ARRIVED && screen.isReturning());
  CHECK(near_m(gps_sim_position(clock.now_s), base, 50));
  // The missions reloaded under the arrival: back to browsing, and the next
  // guidance is not frozen by the arrival left behind
  screen.reloadMissions();
  CHECK(!screen.isGuiding() && !screen.isReturning());
  CHECK(screen.openMission(0) && screen.startGuidance());
  CHECK(screen.lastFix().state != GUIDANCE_ARRIVED && !screen.isReturning());
  // Ending leg 1 before the site: back to the mission overview
  screen.endGuidance();
  CHECK(!screen.isGuiding() && screen.navigationPoints().size() > 0);
  render();

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
