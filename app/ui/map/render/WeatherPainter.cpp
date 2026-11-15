#include "ui/map/render/WeatherPainter.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdio>
#include <limits>

#include "data/data.hpp"

namespace mars::ui::render {

	namespace {


		constexpr std::uint32_t kGridPx = 16;
		/// Fixed scale: colours keep their meaning over the sol.
		constexpr float kColdK = 180.0F;
		constexpr float kWarmK = 260.0F;
		constexpr float kKelvinToCelsius = 273.15F;
		constexpr float kIsothermK = 2.0F;
		constexpr float kIsothermDarken = 0.55F;
		/// ColorBrewer RdYlBu, reversed.
		constexpr Rgb kRamp[] = {rgb(0x4575B4), rgb(0x91BFDB), rgb(0xE0F3F8), rgb(0xFC8D59), rgb(0xD73027)};
		constexpr float kReliefShare = 0.35F;
		constexpr float kDeciKelvin = 10.0F;
		constexpr std::int64_t kUdeg = UDEG_PER_DEG;
		constexpr std::int64_t kHalfCell = kUdeg / 2;
		constexpr float kNoTemperature = std::numeric_limits<float>::quiet_NaN();

		constexpr std::uint32_t kArrowGridPx = 90;
		constexpr float kArrowMinPx = 10.0F;
		constexpr float kArrowMaxPx = 34.0F;
		constexpr float kArrowFullMs = 12.0F;
		constexpr float kArrowHeadPx = 5.0F;
		constexpr float kArrowHeadRad = 2.7F;

		constexpr int kArrowWidth = 1;
		constexpr float kShadowOffset = 1.0F;
		constexpr Rgb kArrowColor = rgb(0xFFFFFF);
		constexpr Rgb kArrowShadow = rgb(0x1B1B1B);
		constexpr float kCmPerM = 100.0F;
		constexpr float kRadPerTurnStep = 2.0F * 3.14159265F / static_cast<float>(WEATHER_TURN_STEPS);

		/// Arrow pointing where the wind blows to.
		void drawWindArrow(Canvas& canvas, float x, float y, const t_weather& w)
		{
			const float speed = static_cast<float>(w.wind_cms) / kCmPerM;
			const float length = std::clamp(kArrowMinPx + (kArrowMaxPx - kArrowMinPx) * speed / kArrowFullMs, kArrowMinPx, kArrowMaxPx);

			const float toward = static_cast<float>(w.wind_from_x256) * kRadPerTurnStep + 3.14159265F;
			const float dx = std::sin(toward);
			const float dy = -std::cos(toward);
			const float x0 = x - dx * length / 2;
			const float y0 = y - dy * length / 2;
			const float x1 = x + dx * length / 2;
			const float y1 = y + dy * length / 2;
			const auto head = [&](float side) {
				const float a = toward + side * kArrowHeadRad;
				return std::pair{x1 + std::sin(a) * kArrowHeadPx, y1 - std::cos(a) * kArrowHeadPx};
			};
			const auto [lx, ly] = head(1.0F);
			const auto [rx, ry] = head(-1.0F);
			for (const auto& [offset, color] : {std::pair{kShadowOffset, kArrowShadow}, std::pair{0.0F, kArrowColor}}) {
				canvas.line(x0 + offset, y0 + offset, x1 + offset, y1 + offset, color, kArrowWidth);
				canvas.line(x1 + offset, y1 + offset, lx + offset, ly + offset, color, kArrowWidth);
				canvas.line(x1 + offset, y1 + offset, rx + offset, ry + offset, color, kArrowWidth);
			}
		}

		float kelvinAt(t_coord c, std::uint32_t time) noexcept
		{
			const t_weather w = weather_estimate(c, time);
			return w.valid ? static_cast<float>(w.temperature_dk) / kDeciKelvin : kNoTemperature;
		}

		/// Bilinear between the four surrounding 1 degree cell centres.
		float interpolated(t_coord c, std::uint32_t time) noexcept
		{
			const std::int64_t fx = std::int64_t{c.lon_udeg} - kHalfCell;
			const std::int64_t fy = std::int64_t{c.lat_udeg} + LAT_MAX_UDEG - kHalfCell;
			const std::int64_t col = (fx >= 0 ? fx : fx - kUdeg + 1) / kUdeg;
			const std::int64_t row = (fy >= 0 ? fy : fy - kUdeg + 1) / kUdeg;
			const float tx = static_cast<float>(fx - col * kUdeg) / static_cast<float>(kUdeg);
			const float ty = static_cast<float>(fy - row * kUdeg) / static_cast<float>(kUdeg);
			float v[4];
			for (std::uint32_t k = 0; k < 4; ++k) {
				const std::int64_t lon = (col + (k & 1U)) * kUdeg + kHalfCell;
				const std::int64_t lat = std::clamp<std::int64_t>((row + (k >> 1U)) * kUdeg + kHalfCell - LAT_MAX_UDEG,
					-LAT_MAX_UDEG, LAT_MAX_UDEG);
				v[k] = kelvinAt({lon_wrap_udeg(lon), static_cast<std::int32_t>(lat)}, time);
			}
			const float here = kelvinAt(c, time);
			for (float& value : v) {
				if (std::isnan(value)) {
					value = here;
				}
			}
			const float top = v[0] * (1.0F - tx) + v[1] * tx;
			const float bottom = v[2] * (1.0F - tx) + v[3] * tx;
			return top * (1.0F - ty) + bottom * ty;
		}

	}

	void WeatherPainter::paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas)
	{
		const std::uint32_t gw = canvas.width() / kGridPx + 2;
		const std::uint32_t gh = canvas.height() / kGridPx + 2;
		grid_.assign(std::size_t{gw} * gh, kNoTemperature);
		for (std::uint32_t gy = 0; gy < gh; ++gy) {
			for (std::uint32_t gx = 0; gx < gw; ++gx) {
				const std::uint32_t x = std::min(gx * kGridPx, canvas.width() - 1);
				const std::uint32_t y = std::min(gy * kGridPx, canvas.height() - 1);
				grid_[std::size_t{gy} * gw + gx] = interpolated(samples.coord(x, y), timeSeconds);
			}
		}
		const auto temperature = [&](std::uint32_t x, std::uint32_t y) {
			const std::uint32_t gx = x / kGridPx;
			const std::uint32_t gy = y / kGridPx;
			const float tx = static_cast<float>(x % kGridPx) / kGridPx;
			const float ty = static_cast<float>(y % kGridPx) / kGridPx;
			const float* const row0 = &grid_[std::size_t{gy} * gw + gx];
			const float* const row1 = row0 + gw;
			return (row0[0] * (1 - tx) + row0[1] * tx) * (1 - ty) + (row1[0] * (1 - tx) + row1[1] * tx) * ty;
		};

		for (std::uint32_t y = 0; y < canvas.height(); ++y) {
			for (std::uint32_t x = 0; x < canvas.width(); ++x) {
				const float t = temperature(x, y);
				if (std::isnan(t)) {
					canvas.set(x, y, onHatch(x, y) ? kNoDataHatch : kNoData);
					continue;
				}
				const std::size_t i = samples.index(x, y);
				const float shade = std::isnan(samples.elevation(i)) ? 1.0F : samples.shade(i);
				Rgb color = scale(ramp(kRamp, (t - kColdK) / (kWarmK - kColdK)), 1.0F - kReliefShare + kReliefShare * shade);

				const float band = std::floor(t / kIsothermK);
				const bool line = (x + 1 < canvas.width() && std::floor(temperature(x + 1, y) / kIsothermK) != band)
					|| (y + 1 < canvas.height() && std::floor(temperature(x, y + 1) / kIsothermK) != band);
				if (line) {
					color = scale(color, kIsothermDarken);
				}
				canvas.set(x, y, color);
			}
		}


		for (std::uint32_t y = kArrowGridPx / 2; y < canvas.height(); y += kArrowGridPx) {
			for (std::uint32_t x = kArrowGridPx / 2; x < canvas.width(); x += kArrowGridPx) {
				const t_weather w = weather_estimate(samples.coord(x, y), timeSeconds);
				if (w.valid) {
					drawWindArrow(canvas, static_cast<float>(x), static_cast<float>(y), w);
				}
			}
		}

		legend_ = {};
		legend_.title = "Air temperature and wind, 2 m above ground";
		legend_.hasGradient = true;
		for (std::size_t k = 0; k < kLegendGradientStops; ++k) {
			legend_.gradient[k] = hexOf(kRamp[k]);
		}
		std::snprintf(legend_.low.data(), legend_.low.size(), "%.0f \xC2\xB0""C", static_cast<double>(kColdK - kKelvinToCelsius));
		std::snprintf(legend_.high.data(), legend_.high.size(), "%.0f \xC2\xB0""C", static_cast<double>(kWarmK - kKelvinToCelsius));
		const t_coord centre = samples.coord(canvas.width() / 2, canvas.height() / 2);
		const t_weather w = weather_estimate(centre, timeSeconds);
		if (w.valid) {
			std::snprintf(legend_.note.data(), legend_.note.size(),
				"Centre %.0f \xC2\xB0""C, wind %.1f m/s from %03u\xC2\xB0, dust opacity %.2f\n"
				"Arrows: where the wind blows, longer when stronger. Isotherms every 2 K. %s",
				static_cast<double>(w.temperature_dk) / 10.0 - static_cast<double>(kKelvinToCelsius), w.wind_cms / 100.0,
				static_cast<unsigned>((w.wind_from_x256 * 360U + 128U) / 256U % 360U), w.dust_x40 / 40.0, source_);
		} else {
			std::snprintf(legend_.note.data(), legend_.note.size(), "No weather data here. %s", source_);
		}
	}

	static_assert(std::size(kRamp) == kLegendGradientStops);
	static_assert(kColdK < kWarmK);

}
