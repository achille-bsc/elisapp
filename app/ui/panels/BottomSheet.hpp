#pragma once

#include <lvgl.h>

namespace mars::ui {

	/// The orange panel rising from the bottom of the screen. Its handle folds
	/// it down to a thin bar and back; subclasses-free: owners fill content().
	class BottomSheet {
	public:
		explicit BottomSheet(lv_obj_t* parent);
		BottomSheet(const BottomSheet&) = delete;
		BottomSheet& operator=(const BottomSheet&) = delete;

		/// The column to which the owner adds its widgets.
		[[nodiscard]] lv_obj_t* content() const noexcept { return content_; }

		/// Showing the sheet also unfolds it.
		void setVisible(bool visible);
		void setCollapsed(bool collapsed);

	private:
		static void onHandle(lv_event_t* event);

		lv_obj_t* root_;
		lv_obj_t* content_;
	};

}
