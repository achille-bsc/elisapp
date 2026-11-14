#pragma once

#include <span>

#include <lvgl.h>

#include "core/mission/Mission.hpp"
#include "ui/panels/BottomSheet.hpp"

namespace mars::ui {

	/// Told what the user chose in the mission list.
	class IMissionListListener {
	public:
		virtual void onMissionSelected(const Mission& mission) = 0;

		/// The user wants to pick the destination directly on the map.
		virtual void onPinpointRequested() = 0;

	protected:
		~IMissionListListener() = default;
	};

	/// The sheet opened by the search field: "choose a pinpoint destination",
	/// then the missions proposed by mission control.
	class MissionListSheet {
	public:
		MissionListSheet(lv_obj_t* parent, IMissionListListener& listener);
		MissionListSheet(const MissionListSheet&) = delete;
		MissionListSheet& operator=(const MissionListSheet&) = delete;

		/// Replaces the rows. `missions` must outlive them: only the pointers are kept.
		void setMissions(std::span<const Mission> missions);

		void setVisible(bool visible) { sheet_.setVisible(visible); }

	private:
		static void onPinpoint(lv_event_t* event);
		static void onRow(lv_event_t* event);

		IMissionListListener& listener_;
		BottomSheet sheet_;
		lv_obj_t* list_;
	};

}
