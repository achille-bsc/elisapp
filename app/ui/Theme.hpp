#pragma once

#include <cstdint>

#include <lvgl.h>

namespace mars::ui::theme {

	// Flight-display palette (0xRRGGBB), the shapes of the Figma design kept:
	// near-black panels, white figures, one cyan accent, red for stop/back.
	inline constexpr std::uint32_t kPanel = 0x0B1016;      ///< Sheets, banners.
	inline constexpr std::uint32_t kPanelLine = 0x2A3440;  ///< Panel borders.
	inline constexpr std::uint32_t kSurface = 0x161E27;    ///< Pills, cards.
	inline constexpr std::uint32_t kSurfacePressed = 0x24303C;
	inline constexpr std::uint32_t kAccent = 0x4FC3F7;     ///< Selected, primary.
	inline constexpr std::uint32_t kOnAccent = 0x0B1016;   ///< Text on the accent.
	inline constexpr std::uint32_t kBack = 0xE5484D;
	inline constexpr std::uint32_t kBackPressed = 0xB83A3E;
	inline constexpr std::uint32_t kText = 0xFFFFFF;
	inline constexpr std::uint32_t kHint = 0x8A97A6;
	inline constexpr std::uint32_t kOnMap = 0xFFFFFF;
	inline constexpr std::uint32_t kImagePlaceholder = 0x24303C;
	/// Route out (solid) and back to the base (dashed).
	inline constexpr std::uint32_t kRouteOut = 0xFFFFFF;
	inline constexpr std::uint32_t kRouteBack = 0x7FD6FF;
	/// The crew's position, as on Earth navigators: blue dot, white ring.
	inline constexpr std::uint32_t kPosition = 0x1A73E8;

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
		lv_style_t backPressed;
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
