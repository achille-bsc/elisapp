#pragma once

#include <lvgl.h>

#include "core/mission/Mission.hpp"

namespace mars::ui {

	/// Writes the one-line summary of `mission` into `label`:
	/// "name | 30 min | Difficulty: 60 % | Type: Geology". A mission without a
	/// duration (a destination picked on the map) shows its name and type only.
	void setMissionSummary(lv_obj_t* label, const Mission& mission);

}
