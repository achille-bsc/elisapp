#pragma once

#include <lvgl.h>

#include "core/mission/Mission.hpp"
#include "core/route/TripPlanner.hpp"
#include "ui/panels/BottomSheet.hpp"

namespace mars::ui {

	/// Told what the user does in the mission sheet.
	class IMissionDetailListener {
	public:
		virtual void onMissionClosed() = 0;
		virtual void onWeatherReportRequested(const Mission& mission) = 0;
		virtual void onGuidanceStartRequested() = 0;

	protected:
		~IMissionDetailListener() = default;
	};

	/// The sheet shown while a trip is displayed: summary of the mission, the
	/// round trip on foot and by rover, risk of the way out and back, Start
	/// (guidance) and weather report buttons, and description.
	class MissionDetailSheet {
	public:
		MissionDetailSheet(lv_obj_t* parent, IMissionDetailListener& listener);
		MissionDetailSheet(const MissionDetailSheet&) = delete;
		MissionDetailSheet& operator=(const MissionDetailSheet&) = delete;

		/// Fills the sheet with `mission`, which must stay valid while it is shown,
		/// and the round trip planned to it.
		void show(const Mission& mission, const TripSummary& trip);
		/// The trip was planned again (the crew moved).
		void setTrip(const TripSummary& trip);
		void hide();

	private:
		static void onBack(lv_event_t* event);
		static void onWeather(lv_event_t* event);
		static void onStart(lv_event_t* event);

		IMissionDetailListener& listener_;
		BottomSheet sheet_;
		lv_obj_t* summary_;
		lv_obj_t* walk_;
		lv_obj_t* rover_;
		lv_obj_t* risk_;
		lv_obj_t* start_;
		lv_obj_t* description_;
		const Mission* mission_{nullptr};
	};

}
