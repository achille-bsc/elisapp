#include "./gps_sim.hpp"
#include "../guidance/guidance.hpp"

// The route geometry (copy, distances, point at a distance) is the guidance's
static t_guidance g_route;
static mars::t_position g_fix = {};
static uint32_t g_speed_mm_s = 0;
static uint32_t g_start_time_s = 0;
static uint16_t g_sequence = 0;
static bool g_walking = false;

void gps_sim_init(mars::t_position fix) {
  g_fix = fix;
  g_walking = false;
  guidance_stop(&g_route);
}

void gps_sim_follow(const mars::t_position *route, uint32_t count,
                    uint32_t speed_mm_s, uint32_t start_time_s) {
  g_walking = guidance_start(&g_route, route, count, speed_mm_s);
  g_speed_mm_s = speed_mm_s;
  g_start_time_s = start_time_s;
}

mars::t_position gps_sim_position(uint32_t time_s) {
  if (!g_walking)
    return (g_fix);
  const uint64_t elapsed_s = time_s > g_start_time_s ? time_s - g_start_time_s : 0;
  const uint64_t along = elapsed_s * g_speed_mm_s;
  return (guidance_position_of(guidance_point_at(
      &g_route, static_cast<uint32_t>(along > UINT32_MAX ? UINT32_MAX : along))));
}

void gps_sim_stop(uint32_t time_s) {
  g_fix = gps_sim_position(time_s);
  g_walking = false;
}

mars::PositionPacket::Buffer gps_sim_packet(uint32_t time_s) {
  return (mars::PositionPacket::encode(gps_sim_position(time_s), g_sequence++));
}

static_assert(mars::PositionPacket::kSize == 16, "CCSDS position packet");
