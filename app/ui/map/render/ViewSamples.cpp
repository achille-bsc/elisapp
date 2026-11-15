#include "ui/map/render/ViewSamples.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "data/data.hpp"

namespace mars::ui::render {

	namespace {

		constexpr float kMmPerM = 1000.0F;
		constexpr double kE7PerUdeg = 10.0;

		constexpr float kLightAzimuthRad = 315.0F * std::numbers::pi_v<float> / 180.0F;
		constexpr float kLightAltitudeRad = 45.0F * std::numbers::pi_v<float> / 180.0F;
		constexpr float kShadeMax = 1.6F;

		constexpr double kMetresPerDegree =
			static_cast<double>(MM_PER_UDEG_LAT_Q16) / Q16_ONE * UDEG_PER_DEG / 1000.0;

	}

	bool ViewSamples::update(const MapProjection& projection)
	{
		const MapSize size = projection.size();
		if (size.width <= 0 || size.height <= 0) {
			width_ = height_ = 0;
			return false;
		}
		const auto width = static_cast<std::uint32_t>(size.width);
		const auto height = static_cast<std::uint32_t>(size.height);
		if (width == width_ && height == height_ && projection.pixelsPerDegree() == pixelsPerDegree_
			&& projection.center().latitudeE7 == center_.latitudeE7 && projection.center().longitudeEastE7 == center_.longitudeEastE7) {
			return false;
		}
		width_ = width;
		height_ = height;
		center_ = projection.center();
		pixelsPerDegree_ = projection.pixelsPerDegree();
		metresPerPixel_ = kMetresPerDegree / pixelsPerDegree_;


		lonUdeg_.resize(width_);
		latUdeg_.resize(height_);
		for (std::uint32_t x = 0; x < width_; ++x) {
			const t_position p = projection.toPosition({static_cast<std::int32_t>(x), size.height / 2});
			lonUdeg_[x] = coord_from_e7_east(0, p.longitudeEastE7).lon_udeg;
		}
		for (std::uint32_t y = 0; y < height_; ++y) {
			const t_position p = projection.toPosition({size.width / 2, static_cast<std::int32_t>(y)});
			latUdeg_[y] = static_cast<std::int32_t>(std::lround(p.latitudeE7 / kE7PerUdeg));
		}

		const std::size_t count = std::size_t{width_} * height_;
		elevation_.resize(count);
		grey_.resize(count);
		lowest_ = std::numeric_limits<float>::max();
		highest_ = std::numeric_limits<float>::lowest();
		hasImagery_ = false;
		for (std::uint32_t y = 0; y < height_; ++y) {
			for (std::uint32_t x = 0; x < width_; ++x) {
				const std::size_t i = index(x, y);
				const t_coord c = coord(x, y);
				std::int32_t mm = 0;
				if (landform_sample_mm(c, &mm)) {
					const float m = static_cast<float>(mm) / kMmPerM;
					elevation_[i] = m;
					lowest_ = std::min(lowest_, m);
					highest_ = std::max(highest_, m);
				} else {
					elevation_[i] = std::numeric_limits<float>::quiet_NaN();
				}
				std::uint16_t grey = kNoGrey;
				if (imagery_sample(c, &grey)) {
					hasImagery_ = true;
				} else {
					grey = kNoGrey;
				}
				grey_[i] = grey;
			}
		}
		if (lowest_ > highest_) {
			lowest_ = highest_ = 0.0F;
		}
		shadeRelief();
		return true;
	}

	void ViewSamples::shadeRelief()
	{
		shade_.assign(elevation_.size(), 1.0F);
		const auto spacing = static_cast<float>(metresPerPixel_);

		const float lx = std::sin(kLightAzimuthRad) * std::cos(kLightAltitudeRad);
		const float ly = std::cos(kLightAzimuthRad) * std::cos(kLightAltitudeRad);
		const float lz = std::sin(kLightAltitudeRad);
		for (std::uint32_t y = 0; y < height_; ++y) {
			for (std::uint32_t x = 0; x < width_; ++x) {
				const std::size_t i = index(x, y);
				const float z = elevation_[i];
				if (std::isnan(z)) {
					continue;
				}
				// Horn 3 x 3 gradient, holes take the centre value
				const auto at = [&](std::int32_t dx, std::int32_t dy) {
					const std::int64_t nx = std::int64_t{x} + dx;
					const std::int64_t ny = std::int64_t{y} + dy;
					if (nx < 0 || ny < 0 || nx >= width_ || ny >= height_) {
						return z;
					}
					const float v = elevation_[index(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny))];
					return std::isnan(v) ? z : v;
				};
				const float east = (at(1, -1) + 2.0F * at(1, 0) + at(1, 1)) - (at(-1, -1) + 2.0F * at(-1, 0) + at(-1, 1));

				const float north = (at(-1, -1) + 2.0F * at(0, -1) + at(1, -1)) - (at(-1, 1) + 2.0F * at(0, 1) + at(1, 1));
				const float dzdx = east / (8.0F * spacing);
				const float dzdy = north / (8.0F * spacing);

				const float norm = std::sqrt(dzdx * dzdx + dzdy * dzdy + 1.0F);
				const float lit = (-dzdx * lx - dzdy * ly + lz) / norm;
				shade_[i] = std::clamp(lit / lz, 0.0F, kShadeMax);
			}
		}
	}

}
