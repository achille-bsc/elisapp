#pragma once

#include <array>
#include <cstdint>

#include <lvgl.h>

namespace mars::ui {

	/// North arrow and the measured heading (true north).
	class CompassChip {
	public:
		explicit CompassChip(lv_obj_t* parent);
		CompassChip(const CompassChip&) = delete;
		CompassChip& operator=(const CompassChip&) = delete;

		void setHeading(bool known, std::uint16_t degrees);
		void setVisible(bool visible);

	private:
		lv_obj_t* root_;
		lv_obj_t* text_;
		std::array<char, 32> buffer_{};
	};

}
