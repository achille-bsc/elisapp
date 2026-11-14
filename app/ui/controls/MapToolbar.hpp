#pragma once

#include <cstdint>

#include <lvgl.h>

namespace mars::ui {

	/// Told when the user presses a control of the toolbar.
	class IToolbarListener {
	public:
		virtual void onLayersPressed() = 0;
		virtual void onSearchPressed() = 0;
		virtual void onRecenterPressed() = 0;

	protected:
		~IToolbarListener() = default;
	};

	/// The row at the bottom of the map: layers button, search field and
	/// "centre on me" button.
	class MapToolbar {
	public:
		MapToolbar(lv_obj_t* parent, IToolbarListener& listener);
		MapToolbar(const MapToolbar&) = delete;
		MapToolbar& operator=(const MapToolbar&) = delete;

		/// Compact: only the layers button, on the left (a sheet is open below).
		void setCompact(bool compact);

		/// Distance from the left edge of the toolbar to its layers button.
		[[nodiscard]] std::int32_t layersButtonOffset() const;

		/// Replaces the text of the search field. `text` is not copied.
		void setSearchText(const char* text);

	private:
		static void onLayers(lv_event_t* event);
		static void onSearch(lv_event_t* event);
		static void onRecenter(lv_event_t* event);

		IToolbarListener& listener_;
		lv_obj_t* root_;
		lv_obj_t* layers_;
		lv_obj_t* search_;
		lv_obj_t* searchLabel_;
		lv_obj_t* recenter_;
	};

}
