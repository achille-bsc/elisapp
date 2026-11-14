#pragma once

#include <cstdint>

#include <lvgl.h>

namespace mars::ui::theme {

	// Colours of the Figma design (0xRRGGBB).
	inline constexpr std::uint32_t kSurface = 0xF0E7E5;
	inline constexpr std::uint32_t kSurfacePressed = 0xDCCFCB;
	inline constexpr std::uint32_t kAccent = 0xE87B0E;
	inline constexpr std::uint32_t kBack = 0xB5390B;
	inline constexpr std::uint32_t kText = 0x3B1206;
	inline constexpr std::uint32_t kHint = 0x6F5F5C;
	inline constexpr std::uint32_t kOnMap = 0xFFFFFF;
	inline constexpr std::uint32_t kImagePlaceholder = 0xCDBBB5;

	// Metrics in pixels. Every touch target is at least kTouchTarget high and
	// wide (ISO 9241-410 / WCAG 2.5.5 recommend 44 px or more).
	inline constexpr std::int32_t kTouchTarget = 56;
	inline constexpr std::int32_t kScreenMargin = 38;
	inline constexpr std::int32_t kGap = 12;
	inline constexpr std::int32_t kSheetRadius = 22;
	inline constexpr std::int32_t kCardRadius = 28;
	inline constexpr std::int32_t kMarkerLabelGap = 6;

	/// Styles shared by every widget: one allocation each, instead of local
	/// style properties duplicated on every object.
	struct Styles {
		lv_style_t screen;
		lv_style_t pill;
		lv_style_t pressed;
		lv_style_t checked;
		lv_style_t sheet;
		lv_style_t card;
		lv_style_t backButton;
		lv_style_t hint;
		lv_style_t smallText;
		lv_style_t onMapText;
		lv_style_t onMapTitle;
		lv_style_t marker;
		lv_style_t markerDot;
	};

	/// Built on first use, which must happen after lv_init().
	[[nodiscard]] const Styles& styles();

}
