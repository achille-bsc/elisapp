#include "ui/map/MapLegend.hpp"

#include <cmath>
#include <cstdio>

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kWidth = 300;
		constexpr std::int32_t kPadding = 14;
		constexpr std::int32_t kSwatch = 18;
		constexpr std::int32_t kGradientHeight = 12;
		constexpr std::int32_t kScaleHeight = 4;
		/// Longest scale bar (a 1-2-5 length).
		constexpr double kScaleMaxPx = 150.0;
		constexpr double kMetresPerKm = 1000.0;
		constexpr std::uint32_t kHatchColor = theme::kPanel;
		constexpr lv_opa_t kHatchOpa = LV_OPA_50;

		constexpr lv_point_precise_t kHatchLine[] = {{0, kSwatch}, {kSwatch, 0}};

		lv_obj_t* smallLabel(lv_obj_t* parent)
		{
			lv_obj_t* const label = widgets::label(parent, "");
			lv_obj_add_style(label, &theme::styles().smallText, LV_PART_MAIN);
			return label;
		}

	}

	MapLegend::MapLegend(lv_obj_t* parent)
		: root_{widgets::box(parent, &theme::styles().card)}
	{
		lv_obj_set_size(root_, kWidth, LV_SIZE_CONTENT);
		lv_obj_align(root_, LV_ALIGN_TOP_RIGHT, -theme::kScreenMargin / 2, theme::kScreenMargin / 2);
		lv_obj_set_style_pad_all(root_, kPadding, LV_PART_MAIN);
		lv_obj_set_style_radius(root_, theme::kSheetRadius, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(root_, LV_OPA_90, LV_PART_MAIN);
		lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(root_, theme::kGap / 2, LV_PART_MAIN);

		title_ = smallLabel(root_);
		lv_obj_set_width(title_, lv_pct(100));
		lv_label_set_long_mode(title_, LV_LABEL_LONG_MODE_WRAP);

		for (Row& row : rows_) {
			row.root = widgets::box(root_);
			lv_obj_set_size(row.root, lv_pct(100), LV_SIZE_CONTENT);
			widgets::flex(row.root, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, theme::kGap / 2);
			row.swatch = widgets::box(row.root);
			lv_obj_set_size(row.swatch, kSwatch, kSwatch);
			lv_obj_set_style_bg_opa(row.swatch, LV_OPA_COVER, LV_PART_MAIN);
			lv_obj_set_style_radius(row.swatch, 3, LV_PART_MAIN);
			row.hatch = lv_line_create(row.swatch);
			lv_line_set_points(row.hatch, kHatchLine, 2);
			lv_obj_set_style_line_width(row.hatch, 3, LV_PART_MAIN);
			lv_obj_set_style_line_color(row.hatch, lv_color_hex(kHatchColor), LV_PART_MAIN);
			lv_obj_set_style_line_opa(row.hatch, kHatchOpa, LV_PART_MAIN);
			row.label = smallLabel(row.root);
			lv_obj_set_flex_grow(row.label, 1);
			lv_label_set_long_mode(row.label, LV_LABEL_LONG_MODE_WRAP);
		}

		gradient_ = widgets::box(root_);
		lv_obj_set_size(gradient_, lv_pct(100), kGradientHeight);
		widgets::flex(gradient_, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, 0);
		for (lv_obj_t*& part : gradientParts_) {
			part = widgets::box(gradient_);
			lv_obj_set_height(part, kGradientHeight);
			lv_obj_set_flex_grow(part, 1);
			lv_obj_set_style_bg_opa(part, LV_OPA_COVER, LV_PART_MAIN);
			lv_obj_set_style_bg_grad_dir(part, LV_GRAD_DIR_HOR, LV_PART_MAIN);
		}
		range_ = widgets::box(root_);
		lv_obj_set_size(range_, lv_pct(100), LV_SIZE_CONTENT);
		low_ = smallLabel(range_);
		high_ = smallLabel(range_);
		lv_obj_align(high_, LV_ALIGN_TOP_RIGHT, 0, 0);

		note_ = smallLabel(root_);
		lv_obj_set_width(note_, lv_pct(100));
		lv_label_set_long_mode(note_, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_add_style(note_, &theme::styles().hint, LV_PART_MAIN);

		scaleBar_ = widgets::box(root_);
		lv_obj_set_height(scaleBar_, kScaleHeight);
		lv_obj_set_style_bg_color(scaleBar_, lv_color_hex(theme::kText), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(scaleBar_, LV_OPA_COVER, LV_PART_MAIN);
		scaleLabel_ = smallLabel(root_);
		widgets::setVisible(root_, false);
	}

	void MapLegend::setVisible(bool visible)
	{
		widgets::setVisible(root_, visible);
	}

	void MapLegend::show(const LegendSpec& spec, double metresPerPixel)
	{
		// The labels point into this copy
		spec_ = spec;
		lv_label_set_text_static(title_, spec_.title);
		for (std::size_t i = 0; i < rows_.size(); ++i) {
			const Row& row = rows_[i];
			const bool used = i < spec_.itemCount;
			widgets::setVisible(row.root, used);
			if (!used) {
				continue;
			}
			const LegendItem& item = spec_.items[i];
			lv_obj_set_style_bg_color(row.swatch, lv_color_hex(item.color), LV_PART_MAIN);
			widgets::setVisible(row.hatch, item.pattern == LegendPattern::Hatched);
			lv_label_set_text_static(row.label, item.label);
		}
		widgets::setVisible(gradient_, spec_.hasGradient);
		widgets::setVisible(range_, spec_.hasGradient);
		if (spec_.hasGradient) {
			for (std::size_t k = 0; k < gradientParts_.size(); ++k) {
				lv_obj_set_style_bg_color(gradientParts_[k], lv_color_hex(spec_.gradient[k]), LV_PART_MAIN);
				lv_obj_set_style_bg_grad_color(gradientParts_[k], lv_color_hex(spec_.gradient[k + 1]), LV_PART_MAIN);
			}
			lv_label_set_text_static(low_, spec_.low.data());
			lv_label_set_text_static(high_, spec_.high.data());
		}
		lv_label_set_text_static(note_, spec_.note.data());
		showScale(metresPerPixel);
	}

	void MapLegend::showScale(double metresPerPixel)
	{
		if (!(metresPerPixel > 0.0)) {
			widgets::setVisible(scaleBar_, false);
			return;
		}
		const double most = kScaleMaxPx * metresPerPixel;
		double length = std::pow(10.0, std::floor(std::log10(most)));
		for (const double step : {5.0, 2.0}) {
			if (length * step <= most) {
				length *= step;
				break;
			}
		}
		lv_obj_set_width(scaleBar_, static_cast<std::int32_t>(std::lround(length / metresPerPixel)));
		widgets::setVisible(scaleBar_, true);
		if (length >= kMetresPerKm) {
			std::snprintf(scaleText_.data(), scaleText_.size(), "%.0f km", length / kMetresPerKm);
		} else {
			std::snprintf(scaleText_.data(), scaleText_.size(), "%.0f m", length);
		}
		lv_label_set_text_static(scaleLabel_, scaleText_.data());
	}

}
