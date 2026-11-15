#include "ui/map/render/RiskPainter.hpp"

#include <array>
#include <cmath>
#include <cstdio>

#include "data/data.hpp"

namespace mars::ui::render {

	namespace {


		constexpr std::uint32_t kRiskStep = 2;
		constexpr float kGreyFull = 255.0F * 256.0F;

		constexpr float kPlainGround = 0.55F;
		constexpr float kHatchDarken = 0.6F;
		constexpr float kBorderDarken = 0.35F;

		struct Style {
			Rgb color;
			float alpha;
			bool danger;
			bool hatched;
			const char* label;
		};

		/// Indexed by RiskCategory; ISO 3864 meanings.
		constexpr std::array<Style, kRiskCategoryCount> kStyles{{
			{rgb(0x009E73), 0.22F, false, false, "Safe on foot"},
			{rgb(0xF0E442), 0.30F, false, true, "Slope uncertain (coarse relief)"},
			{rgb(0xE69F00), 0.55F, false, false, "Caution: slope 15 to 20\xC2\xB0"},
			{rgb(0xE1BE6A), 0.50F, false, false, "Caution: dusty ground"},
			{rgb(0xCC79A7), 0.45F, false, false, "Caution: radiation over 80 % of limit"},
			{rgb(0xD55E00), 0.72F, true, true, "Danger: slope over 20\xC2\xB0"},
			{rgb(0x8C4A00), 0.72F, true, true, "Danger: soft ground"},
			{rgb(0x0072B2), 0.68F, true, true, "Danger: strong wind or dust storm"},
			{rgb(0x882255), 0.70F, true, true, "Danger: radiation or solar storm"},
			{rgb(0x3C3C3C), 0.85F, true, true, "Danger: unknown ground"},
		}};

		[[nodiscard]] const Style& styleOf(RiskCategory category) noexcept
		{
			return kStyles[static_cast<std::size_t>(category)];
		}

	}

	RiskCategory riskCategoryOf(std::uint16_t reasons) noexcept
	{
		if (reasons & HAZARD_NO_TERRAIN) {
			return RiskCategory::UnknownGround;
		}
		if (reasons & HAZARD_TOO_STEEP) {
			return RiskCategory::TooSteep;
		}
		if (reasons & (HAZARD_WIND | HAZARD_DUST_STORM)) {
			return RiskCategory::Atmosphere;
		}
		if (reasons & (HAZARD_RADIATION_CRITICAL | HAZARD_SPE_ALERT)) {
			return RiskCategory::RadiationDanger;
		}
		if (reasons & HAZARD_SOFT_GROUND) {
			return RiskCategory::SoftGround;
		}
		if (reasons & HAZARD_STEEP) {
			return RiskCategory::Steep;
		}
		if (reasons & HAZARD_DUSTY_GROUND) {
			return RiskCategory::DustyGround;
		}
		if (reasons & HAZARD_RADIATION_HIGH) {
			return RiskCategory::RadiationHigh;
		}
		if (reasons & HAZARD_COARSE_TERRAIN) {
			return RiskCategory::Uncertain;
		}
		return RiskCategory::Safe;
	}

	void RiskPainter::paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas)
	{
		const std::uint32_t gw = (canvas.width() + kRiskStep - 1) / kRiskStep;
		const std::uint32_t gh = (canvas.height() + kRiskStep - 1) / kRiskStep;
		grid_.resize(std::size_t{gw} * gh);
		std::array<bool, kRiskCategoryCount> present{};
		for (std::uint32_t gy = 0; gy < gh; ++gy) {
			for (std::uint32_t gx = 0; gx < gw; ++gx) {
				const t_coord c = samples.coord(std::min(gx * kRiskStep, canvas.width() - 1), std::min(gy * kRiskStep, canvas.height() - 1));
				const RiskCategory category = riskCategoryOf(hazard_assess(c, timeSeconds).reasons);
				grid_[std::size_t{gy} * gw + gx] = category;
				present[static_cast<std::size_t>(category)] = true;
			}
		}

		for (std::uint32_t y = 0; y < canvas.height(); ++y) {
			for (std::uint32_t x = 0; x < canvas.width(); ++x) {
				const std::uint32_t gx = x / kRiskStep;
				const std::uint32_t gy = y / kRiskStep;
				const RiskCategory category = grid_[std::size_t{gy} * gw + gx];
				const Style& style = styleOf(category);
				const std::size_t i = samples.index(x, y);

				const std::uint16_t grey = samples.grey(i);
				const float ground = grey == ViewSamples::kNoGrey ? kPlainGround : static_cast<float>(grey) / kGreyFull;
				const float shade = std::isnan(samples.elevation(i)) ? 1.0F : samples.shade(i);
				const std::uint8_t g = channel(ground * (0.6F + 0.4F * shade) * static_cast<float>(kChannelMax));
				Rgb color = blend({g, g, g}, style.color, style.alpha);
				if (style.hatched && onHatch(x, y)) {
					color = scale(style.color, kHatchDarken);
				}
				// Border between categories when one is a danger
				const bool edgeX = x % kRiskStep == kRiskStep - 1 && gx + 1 < gw;
				const bool edgeY = y % kRiskStep == kRiskStep - 1 && gy + 1 < gh;
				const RiskCategory right = edgeX ? grid_[std::size_t{gy} * gw + gx + 1] : category;
				const RiskCategory below = edgeY ? grid_[std::size_t{gy + 1} * gw + gx] : category;
				for (const RiskCategory other : {right, below}) {
					if (other != category && (style.danger || styleOf(other).danger)) {
						const Style& severe = style.danger ? style : styleOf(other);
						color = scale(severe.color, kBorderDarken);
					}
				}
				canvas.set(x, y, color);
			}
		}

		legend_ = {};
		legend_.title = "Risk on foot, now";
		for (std::size_t k = 0; k < kRiskCategoryCount && legend_.itemCount < kLegendMaxItems; ++k) {
			if (!present[k] && k != static_cast<std::size_t>(RiskCategory::Safe)) {
				continue;  // only the causes met in this view
			}
			const Style& s = kStyles[k];
			legend_.items[legend_.itemCount++] = {hexOf(s.color), s.hatched ? LegendPattern::Hatched : LegendPattern::Solid, s.label};
		}
		std::snprintf(legend_.note.data(), legend_.note.size(),
			"Limits: NASA-STD-3001, xEVA suit 20\xC2\xB0.\nTap the map for the causes at a place.");
	}

	static_assert(kStyles.size() == kRiskCategoryCount);
	static_assert(kRiskCategoryCount <= kLegendMaxItems);

}
