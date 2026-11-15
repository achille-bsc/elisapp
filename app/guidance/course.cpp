#include "./course.hpp"
#include "../data/core/geodesy.hpp"
#include "./guidance.hpp"

void course_reset(t_course *course) { *course = t_course{}; }

void course_update(t_course *course, mars::t_position position, uint32_t time_s) {
  const t_coord here =
      coord_from_e7_east(position.latitudeE7, position.longitudeEastE7);
  if (!course->has_anchor || time_s < course->anchor_time_s) {
    course->anchor = here;
    course->anchor_time_s = time_s;
    course->has_anchor = true;
    return;
  }
  const uint32_t moved_mm = coord_distance_mm(course->anchor, here);
  if (moved_mm < COURSE_MIN_MOVE_MM)
    return;
  const uint32_t dt = time_s - course->anchor_time_s;
  course->heading_deg = guidance_heading_deg(course->anchor, here);
  const uint32_t speed = dt ? moved_mm / dt : moved_mm;
  course->speed_mm_s = static_cast<uint16_t>(speed > UINT16_MAX ? UINT16_MAX : speed);
  course->valid = true;
  course->moved_time_s = time_s;
  course->anchor = here;
  course->anchor_time_s = time_s;
}

bool course_known(const t_course *course, uint32_t time_s) {
  return (course->valid && time_s >= course->moved_time_s &&
          time_s - course->moved_time_s <= COURSE_STALE_S);
}

static_assert(sizeof(t_course) == 24, "no hidden padding");
