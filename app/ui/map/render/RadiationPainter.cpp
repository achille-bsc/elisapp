#include "ui/map/render/RadiationPainter.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

#include "data/data.hpp"

namespace mars::ui::render {

	namespace {


		constexpr std::uint32_t kGridPx = 8;
		/// Fixed scale (uSv/sol): GCR alone on Mars is ~0.6-0.7 mSv/sol.
		constexpr float kLowUsv = 500.0F;
		constexpr float kHighUsv = 1300.0F;
		constexpr float kIsolineUsv = 5.0F;
		constexpr float kIsolineDarken = 0.7F;
		constexpr float kLimitDarken = 0.25F;
		constexpr float kReliefShare = 0.35F;
		/// Viridis-like, colour-blind safe.
		constexpr Rgb kRamp[] = {rgb(0x2D1B69), rgb(0x31688E), rgb(0x35B779), rgb(0xB5DE2B), rgb(0xFDE725)};
		constexpr float kNoDose = std::numeric_limits<float>::quiet_NaN();
		constexpr float kUsvPerMsv = 1000.0F;

		float doseAt(t_coord c, std::uint32_t time) noexcept
		{
			const t_radiation r = radiation_from(landform_get(c), weather_estimate(c, time));
			return r.valid ? static_cast<float>(r.dose_eq_usv_day) : kNoDose;
		}

	}

	void RadiationPainter::paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas)
	{
		const std::uint32_t gw = canvas.width() / kGridPx + 2;
		const std::uint32_t gh = canvas.height() / kGridPx + 2;
		grid_.assign(std::size_t{gw} * gh, kNoDose);
		for (std::uint32_t gy = 0; gy < gh; ++gy) {
			for (std::uint32_t gx = 0; gx < gw; ++gx) {
				const std::uint32_t x = std::min(gx * kGridPx, canvas.width() - 1);
				const std::uint32_t y = std::min(gy * kGridPx, canvas.height() - 1);
				grid_[std::size_t{gy} * gw + gx] = doseAt(samples.coord(x, y), timeSeconds);
			}
		}
		const auto dose = [&](std::uint32_t x, std::uint32_t y) {
			const float tx = static_cast<float>(x % kGridPx) / kGridPx;
			const float ty = static_cast<float>(y % kGridPx) / kGridPx;
			const float* const r0 = &grid_[std::size_t{y / kGridPx} * gw + x / kGridPx];
			const float* const r1 = r0 + gw;
			return (r0[0] * (1 - tx) + r0[1] * tx) * (1 - ty) + (r1[0] * (1 - tx) + r1[1] * tx) * ty;
		};
		const auto critical = static_cast<float>(RADIATION_CRITICAL_USV_DAY);
		const float caution = critical * static_cast<float>(RAD_CAUTION_PERCENT) / 100.0F;
		const auto crosses = [](float a, float b, float level) { return (a - level) * (b - level) < 0.0F; };

		for (std::uint32_t y = 0; y < canvas.height(); ++y) {
			for (std::uint32_t x = 0; x < canvas.width(); ++x) {
				const float d = dose(x, y);
				if (std::isnan(d)) {
					canvas.set(x, y, onHatch(x, y) ? kNoDataHatch : kNoData);
					continue;
				}
				const std::size_t i = samples.index(x, y);
				const float shade = std::isnan(samples.elevation(i)) ? 1.0F : samples.shade(i);
				Rgb color = scale(ramp(kRamp, (d - kLowUsv) / (kHighUsv - kLowUsv)), 1.0F - kReliefShare + kReliefShare * shade);
				const float right = x + 1 < canvas.width() ? dose(x + 1, y) : d;
				const float below = y + 1 < canvas.height() ? dose(x, y + 1) : d;
				if (crosses(d, right, caution) || crosses(d, below, caution) || crosses(d, right, critical)
					|| crosses(d, below, critical)) {
					color = scale(color, kLimitDarken);
				} else if (std::floor(d / kIsolineUsv) != std::floor(right / kIsolineUsv)
					|| std::floor(d / kIsolineUsv) != std::floor(below / kIsolineUsv)) {
					color = scale(color, kIsolineDarken);
				}
				canvas.set(x, y, color);
			}
		}

		legend_ = {};
		legend_.title = "Radiation dose rate, now";
		legend_.hasGradient = true;
		for (std::size_t k = 0; k < kLegendGradientStops; ++k) {
			legend_.gradient[k] = hexOf(kRamp[k]);
		}
		std::snprintf(legend_.low.data(), legend_.low.size(), "%.2f mSv/sol", static_cast<double>(kLowUsv / kUsvPerMsv));
		std::snprintf(legend_.high.data(), legend_.high.size(), "%.2f mSv/sol", static_cast<double>(kHighUsv / kUsvPerMsv));
		const float centre = dose(canvas.width() / 2, canvas.height() / 2);
		std::snprintf(legend_.note.data(), legend_.note.size(),
			"Centre %.3f mSv/sol. Isolines every %.0f uSv/sol; bold: caution %.2f, danger %.2f (NASA-STD-3001).",
			static_cast<double>(centre / kUsvPerMsv), static_cast<double>(kIsolineUsv), static_cast<double>(caution / kUsvPerMsv),
			static_cast<double>(critical / kUsvPerMsv));
	}

	static_assert(std::size(kRamp) == kLegendGradientStops);

}
