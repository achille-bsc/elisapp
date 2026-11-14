#include "core/map/MapProjection.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mars {

	namespace {

		constexpr double kE7 = 1e7;
		constexpr std::int64_t kHalfTurnE7 = 1'800'000'000;
		constexpr std::int64_t kFullTurnE7 = 3'600'000'000;
		constexpr std::int64_t kPoleE7 = 900'000'000;

		/// Keeps east-west distances usable close to the poles.
		constexpr double kMinCosLatitude = 0.01;

		/// Far enough to be off any screen, small enough for LVGL coordinates.
		constexpr double kMaxPixel = 1'000'000.0;

		/// Signed shortest longitude difference, in [-180, 180) degrees x 1e7.
		constexpr std::int64_t wrapDeltaE7(std::int64_t deltaE7) noexcept
		{
			deltaE7 %= kFullTurnE7;
			if (deltaE7 >= kHalfTurnE7) {
				return deltaE7 - kFullTurnE7;
			}
			if (deltaE7 < -kHalfTurnE7) {
				return deltaE7 + kFullTurnE7;
			}
			return deltaE7;
		}

		constexpr std::uint32_t wrapLongitudeE7(std::int64_t longitudeE7) noexcept
		{
			longitudeE7 %= kFullTurnE7;
			return static_cast<std::uint32_t>(longitudeE7 < 0 ? longitudeE7 + kFullTurnE7 : longitudeE7);
		}

		double cosLatitude(std::int32_t latitudeE7) noexcept
		{
			const double radians = static_cast<double>(latitudeE7) / kE7 * std::numbers::pi / 180.0;
			return std::max(std::cos(radians), kMinCosLatitude);
		}

		std::int32_t toPixel(double value) noexcept
		{
			return static_cast<std::int32_t>(std::lround(std::clamp(value, -kMaxPixel, kMaxPixel)));
		}

	}

	MapProjection::MapProjection(t_position center, double pixelsPerDegree, MapSize size) noexcept
		: pixelsPerDegree_{std::clamp(pixelsPerDegree, kMinPixelsPerDegree, kMaxPixelsPerDegree)}
		, size_{size}
	{
		setCenter(center);
	}

	MapProjection MapProjection::fitting(std::span<const t_position> positions, MapSize size, std::int32_t margin) noexcept
	{
		if (positions.empty()) {
			return MapProjection{{0, 0}, kDefaultPixelsPerDegree, size};
		}

		// Longitudes are measured from the first position so that a region
		// straddling the prime meridian stays in one piece.
		const std::int64_t referenceE7 = positions.front().longitudeEastE7;
		std::int64_t west = 0;
		std::int64_t east = 0;
		std::int64_t south = positions.front().latitudeE7;
		std::int64_t north = south;
		for (const t_position& position : positions) {
			const std::int64_t delta = wrapDeltaE7(static_cast<std::int64_t>(position.longitudeEastE7) - referenceE7);
			west = std::min(west, delta);
			east = std::max(east, delta);
			south = std::min<std::int64_t>(south, position.latitudeE7);
			north = std::max<std::int64_t>(north, position.latitudeE7);
		}

		const t_position center{
			static_cast<std::int32_t>(std::clamp((south + north) / 2, -kPoleE7, kPoleE7)),
			wrapLongitudeE7(referenceE7 + (west + east) / 2),
		};

		const std::int64_t margins = 2 * std::clamp<std::int64_t>(margin, 0, std::max(size.width, size.height));
		const double usableWidth = static_cast<double>(std::max<std::int64_t>(1, size.width - margins));
		const double usableHeight = static_cast<double>(std::max<std::int64_t>(1, size.height - margins));
		const double spanX = static_cast<double>(east - west) / kE7 * cosLatitude(center.latitudeE7);
		const double spanY = static_cast<double>(north - south) / kE7;

		double pixelsPerDegree = kMaxPixelsPerDegree;
		if (spanX > 0.0) {
			pixelsPerDegree = std::min(pixelsPerDegree, usableWidth / spanX);
		}
		if (spanY > 0.0) {
			pixelsPerDegree = std::min(pixelsPerDegree, usableHeight / spanY);
		}
		if (spanX <= 0.0 && spanY <= 0.0) {
			pixelsPerDegree = kDefaultPixelsPerDegree;
		}
		return MapProjection{center, pixelsPerDegree, size};
	}

	void MapProjection::setCenter(t_position center) noexcept
	{
		center_ = center;
		cosCenterLatitude_ = cosLatitude(center.latitudeE7);
	}

	MapPoint MapProjection::toPoint(t_position position) const noexcept
	{
		const std::int64_t deltaLongitude =
			wrapDeltaE7(static_cast<std::int64_t>(position.longitudeEastE7) - static_cast<std::int64_t>(center_.longitudeEastE7));
		const std::int64_t deltaLatitude = static_cast<std::int64_t>(position.latitudeE7) - center_.latitudeE7;

		const double x = static_cast<double>(deltaLongitude) / kE7 * cosCenterLatitude_ * pixelsPerDegree_;
		const double y = static_cast<double>(deltaLatitude) / kE7 * pixelsPerDegree_;
		return {toPixel(size_.width / 2.0 + x), toPixel(size_.height / 2.0 - y)};
	}

	t_position MapProjection::toPosition(MapPoint point) const noexcept
	{
		const double degreesEast = (point.x - size_.width / 2.0) / (cosCenterLatitude_ * pixelsPerDegree_);
		const double degreesNorth = (size_.height / 2.0 - point.y) / pixelsPerDegree_;

		const std::int64_t latitudeE7 = center_.latitudeE7 + std::llround(degreesNorth * kE7);
		const std::int64_t longitudeE7 = static_cast<std::int64_t>(center_.longitudeEastE7) + std::llround(degreesEast * kE7);
		return {
			static_cast<std::int32_t>(std::clamp(latitudeE7, -kPoleE7, kPoleE7)),
			wrapLongitudeE7(longitudeE7),
		};
	}

}
