#pragma once

#include <lvgl.h>

#include "core/mission/Mission.hpp"
#include "ui/panels/BottomSheet.hpp"

namespace mars::ui {

	/// Told what the user does in the mission sheet.
	class IMissionDetailListener {
	public:
		virtual void onMissionClosed() = 0;
		virtual void onWeatherReportRequested(const Mission& mission) = 0;

	protected:
		~IMissionDetailListener() = default;
	};

	/// The sheet shown while a route is displayed: summary of the mission, travel
	/// times on foot and by rover, weather report button and description.
	class MissionDetailSheet {
	public:
		MissionDetailSheet(lv_obj_t* parent, IMissionDetailListener& listener);
		MissionDetailSheet(const MissionDetailSheet&) = delete;
		MissionDetailSheet& operator=(const MissionDetailSheet&) = delete;

		/// Fills the sheet with `mission`, which must stay valid while it is shown.
		void show(const Mission& mission);
		void hide();

	private:
		static void onBack(lv_event_t* event);
		static void onWeather(lv_event_t* event);

		IMissionDetailListener& listener_;
		BottomSheet sheet_;
		lv_obj_t* summary_;
		lv_obj_t* walk_;
		lv_obj_t* rover_;
		lv_obj_t* description_;
		const Mission* mission_{nullptr};
	};

}
