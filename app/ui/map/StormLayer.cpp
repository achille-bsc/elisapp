#include "ui/map/StormLayer.hpp"

#include <algorithm>
#include <cstdio>

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"
#include "ui/mission_time.hpp"

namespace mars::ui {

	namespace {

		constexpr std::uint32_t kStormColor = 0xE5484D;
		constexpr std::uint32_t kLabelBack = 0x0B1016;
		constexpr std::int32_t kActiveBorder = 6;
		constexpr std::int32_t kComingBorder = 4;
		constexpr std::int32_t kGlow = 8;
		constexpr std::uint32_t kComingColor = 0xF0B429;
		constexpr std::int32_t kLabelPadding = 6;
		constexpr std::int32_t kMinRadiusPx = 6;
		constexpr double kMaxRadiusPx = 4000.0;

	}

	StormLayer::StormLayer(lv_obj_t* map)
	{
		for (std::size_t i = 0; i < kMaxStorms; ++i) {
			lv_obj_t* const disc = widgets::box(map);
			lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, LV_PART_MAIN);
			lv_obj_set_style_bg_color(disc, lv_color_hex(kStormColor), LV_PART_MAIN);
			lv_obj_set_style_border_color(disc, lv_color_hex(kStormColor), LV_PART_MAIN);
			lv_obj_set_style_outline_width(disc, kGlow, LV_PART_MAIN);
			lv_obj_set_style_outline_pad(disc, 0, LV_PART_MAIN);
			lv_obj_set_style_outline_opa(disc, LV_OPA_40, LV_PART_MAIN);
			lv_obj_t* const sign = widgets::label(disc, LV_SYMBOL_WARNING);
			lv_obj_set_style_text_font(sign, &lv_font_montserrat_36, LV_PART_MAIN);
			lv_obj_set_style_text_color(sign, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
			lv_obj_center(sign);
			widgets::setVisible(disc, false);
			discs_[i] = disc;
			lv_obj_t* const label = widgets::label(map, "");
			lv_obj_add_style(label, &theme::styles().smallText, LV_PART_MAIN);
			lv_obj_set_style_text_color(label, lv_color_hex(theme::kOnMap), LV_PART_MAIN);
			lv_obj_set_style_bg_color(label, lv_color_hex(kStormColor), LV_PART_MAIN);
			lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
			lv_obj_set_style_radius(label, kLabelPadding, LV_PART_MAIN);
			lv_obj_set_style_pad_all(label, kLabelPadding, LV_PART_MAIN);
			lv_obj_set_style_translate_x(label, lv_pct(-50), LV_PART_MAIN);
			widgets::setVisible(label, false);
			labels_[i] = label;
		}
	}

	void StormLayer::setStorms(std::span<const StormView> storms)
	{
		count_ = std::min(storms.size(), kMaxStorms);
		for (std::size_t i = 0; i < kMaxStorms; ++i) {
			const bool used = i < count_;
			widgets::setVisible(discs_[i], used);
			widgets::setVisible(labels_[i], used);
			if (!used) {
				continue;
			}
			storms_[i] = storms[i];
			const StormView& s = storms_[i];
			const ShortText from = format_lmst(s.centre, s.startSeconds);
			const ShortText to = format_lmst(s.centre, s.endSeconds);
			std::snprintf(texts_[i].data(), texts_[i].size(), LV_SYMBOL_WARNING " %s  %s %s - %s", s.name,
				s.active ? "now," : "from", from.data(), to.data());
			lv_label_set_text_static(labels_[i], texts_[i].data());
			// Under way: red; forecast: amber (ISO 3864)
			const std::uint32_t color = s.active ? kStormColor : kComingColor;
			lv_obj_set_style_bg_color(discs_[i], lv_color_hex(color), LV_PART_MAIN);
			lv_obj_set_style_border_color(discs_[i], lv_color_hex(color), LV_PART_MAIN);
			lv_obj_set_style_outline_color(discs_[i], lv_color_hex(color), LV_PART_MAIN);
			lv_obj_set_style_bg_color(labels_[i], lv_color_hex(color), LV_PART_MAIN);
			lv_obj_set_style_text_color(labels_[i], lv_color_hex(s.active ? theme::kOnMap : kLabelBack), LV_PART_MAIN);
			lv_obj_set_style_bg_opa(discs_[i], s.active ? LV_OPA_50 : LV_OPA_30, LV_PART_MAIN);
			lv_obj_set_style_border_width(discs_[i], s.active ? kActiveBorder : kComingBorder, LV_PART_MAIN);
			lv_obj_set_style_border_opa(discs_[i], LV_OPA_COVER, LV_PART_MAIN);
		}
	}

	void StormLayer::layout(const MapProjection& projection)
	{
		for (std::size_t i = 0; i < count_; ++i) {
			const StormView& s = storms_[i];
			const MapPoint centre = projection.toPoint(s.centre);
			constexpr double kMetresPerDegree = 59'157.6;
			const double radius = std::clamp(s.radiusM * projection.pixelsPerDegree() / kMetresPerDegree,
				static_cast<double>(kMinRadiusPx), kMaxRadiusPx);
			const auto r = static_cast<std::int32_t>(radius);
			lv_obj_set_size(discs_[i], 2 * r, 2 * r);
			lv_obj_set_pos(discs_[i], centre.x - r, centre.y - r);
			lv_obj_set_pos(labels_[i], centre.x, centre.y - r - theme::kTouchTarget / 2);
		}
	}

}
