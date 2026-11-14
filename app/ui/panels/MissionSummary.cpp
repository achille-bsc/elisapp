#include "ui/panels/MissionSummary.hpp"

namespace mars::ui {

	void setMissionSummary(lv_obj_t* label, const Mission& mission)
	{
		if (mission.durationMinutes == 0) {
			lv_label_set_text_fmt(label, "%s    |    Type: %s", mission.name, mission.type);
			return;
		}
		lv_label_set_text_fmt(label, "%s    |    %u min    |    Difficulty: %u %%    |    Type: %s", mission.name,
			static_cast<unsigned>(mission.durationMinutes), static_cast<unsigned>(mission.difficultyPercent), mission.type);
	}

}
