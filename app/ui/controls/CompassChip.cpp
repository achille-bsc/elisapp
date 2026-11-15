#include "ui/controls/CompassChip.hpp"

#include <cstdio>

#include "guidance/guidance.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::uint32_t kChipColor = theme::kPanel;
		constexpr std::uint32_t kNorthColor = 0xE5484D;
		constexpr std::int32_t kPadding = 10;
		constexpr std::int32_t kBelowZoom = 130;

	}

	CompassChip::CompassChip(lv_obj_t* parent)
		: root_{widgets::box(parent)}
	{
		lv_obj_set_size(root_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
		lv_obj_align(root_, LV_ALIGN_RIGHT_MID, -theme::kScreenMargin, kBelowZoom);
		lv_obj_set_style_bg_color(root_, lv_color_hex(kChipColor), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(root_, LV_OPA_80, LV_PART_MAIN);
		lv_obj_set_style_radius(root_, theme::kGap, LV_PART_MAIN);
		lv_obj_set_style_pad_all(root_, kPadding, LV_PART_MAIN);
		widgets::flex(root_, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_CENTER, 2);
		lv_obj_t* const north = widgets::label(root_, LV_SYMBOL_UP "\nN");
		lv_obj_set_style_text_color(north, lv_color_hex(kNorthColor), LV_PART_MAIN);
		lv_obj_set_style_text_align(north, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		text_ = widgets::label(root_, "");
		lv_obj_set_style_text_color(text_, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
		lv_obj_set_style_text_align(text_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_add_style(text_, &theme::styles().smallText, LV_PART_MAIN);
		setHeading(false, 0);
	}

	void CompassChip::setHeading(bool known, std::uint16_t degrees)
	{
		if (known) {
			std::snprintf(buffer_.data(), buffer_.size(), "HDG\n%03u\xC2\xB0 %s", static_cast<unsigned>(degrees),
				guidance_compass(degrees));
		} else {
			std::snprintf(buffer_.data(), buffer_.size(), "HDG\n---");
		}
		lv_label_set_text_static(text_, buffer_.data());
	}

	void CompassChip::setVisible(bool visible)
	{
		widgets::setVisible(root_, visible);
	}

}
