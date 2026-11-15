#include "ui/map/render/TerrainPainter.hpp"

#include <cmath>
#include <cstdio>

#include "data/data.hpp"

namespace mars::ui::render {

	namespace {


		constexpr Rgb kImageRamp[] = {rgb(0x1F120C), rgb(0x6B3720), rgb(0xB06A3E), rgb(0xD9A173), rgb(0xF4DDBE)};

		constexpr Rgb kReliefRamp[] = {rgb(0x2E1A12), rgb(0x6E3420), rgb(0xA65A33), rgb(0xD39A6B), rgb(0xF0D8B8)};
		/// The image has its own shadows: keep only part of the relief shading.
		constexpr float kImageShadeShare = 0.45F;
		constexpr float kGreyFull = 255.0F * 256.0F;
		constexpr double kMetresPerUdeg = static_cast<double>(MM_PER_UDEG_LAT_Q16) / Q16_ONE / 1000.0;


		void resolutionAt(t_coord c, double& reliefM, double& imageM)
		{
			reliefM = 0.0;
			imageM = 0.0;
			const t_landform land = landform_get(c);
			if (land.valid) {
				reliefM = landform_layer(land.layer)->step_udeg * kMetresPerUdeg;
			}
			for (std::uint8_t i = 0; i < imagery_layer_count(); ++i) {
				const t_imagery_header& h = *imagery_layer(i);
				const std::int64_t col = (std::int64_t{c.lon_udeg} - h.west_udeg) / h.step_udeg;
				const std::int64_t row = (std::int64_t{h.north_udeg} - c.lat_udeg) / h.step_udeg;
				if (c.lon_udeg >= h.west_udeg && c.lat_udeg <= h.north_udeg && col < h.width && row < h.height) {
					imageM = h.step_udeg * kMetresPerUdeg;
					return;  // sorted finest first
				}
			}
		}

		void describeSize(char* text, std::size_t size, double metres)
		{
			if (metres <= 0.0) {
				std::snprintf(text, size, "none");
			} else if (metres < 1000.0) {
				std::snprintf(text, size, "%.0f m", metres);
			} else {
				std::snprintf(text, size, "%.1f km", metres / 1000.0);
			}
		}

	}

	void TerrainPainter::paint(const ViewSamples& samples, std::uint32_t /*timeSeconds*/, Canvas& canvas)
	{
		const float low = samples.lowest();
		const float range = std::max(samples.highest() - low, 1.0F);
		for (std::uint32_t y = 0; y < canvas.height(); ++y) {
			for (std::uint32_t x = 0; x < canvas.width(); ++x) {
				const std::size_t i = samples.index(x, y);
				const float z = samples.elevation(i);
				const std::uint16_t grey = samples.grey(i);
				if (std::isnan(z) && grey == ViewSamples::kNoGrey) {
					canvas.set(x, y, onHatch(x, y) ? kNoDataHatch : kNoData);
					continue;
				}
				const float shade = std::isnan(z) ? 1.0F : samples.shade(i);
				Rgb color{};
				if (grey != ViewSamples::kNoGrey) {
					color = ramp(kImageRamp, static_cast<float>(grey) / kGreyFull);
					color = scale(color, 1.0F - kImageShadeShare + kImageShadeShare * shade);
				} else {
					color = scale(ramp(kReliefRamp, (z - low) / range), shade);
				}
				canvas.set(x, y, color);
			}
		}

		legend_ = {};
		legend_.title = "Terrain";
		legend_.hasGradient = true;
		for (std::size_t k = 0; k < kLegendGradientStops; ++k) {
			legend_.gradient[k] = hexOf(kReliefRamp[k]);
		}
		std::snprintf(legend_.low.data(), legend_.low.size(), "%.0f m", static_cast<double>(samples.lowest()));
		std::snprintf(legend_.high.data(), legend_.high.size(), "%.0f m", static_cast<double>(samples.highest()));
		double reliefM = 0.0;
		double imageM = 0.0;
		resolutionAt(samples.coord(canvas.width() / 2, canvas.height() / 2), reliefM, imageM);
		char relief[16];
		char image[16];
		describeSize(relief, sizeof relief, reliefM);
		describeSize(image, sizeof image, imageM);
		std::snprintf(legend_.note.data(), legend_.note.size(), "Elevation above the MOLA areoid\nRelief grid %s, orbital image %s",
			relief, image);
	}

	static_assert(std::size(kReliefRamp) == kLegendGradientStops);

}
