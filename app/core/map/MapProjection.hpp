#pragma once

#include <cstdint>
#include <span>

#include "core/gps/Position.hpp"

namespace mars {

	/// A pixel of the map view, x to the right and y down from its top-left corner.
	struct MapPoint {
		std::int32_t x;
		std::int32_t y;
	};

	/// Size of the map view in pixels.
	struct MapSize {
		std::int32_t width;
		std::int32_t height;
	};

	/// Equirectangular projection (IAU areographic coordinates: planetocentric
	/// latitude, longitude positive east) of the region shown by a map view.
	/// North is up; east-west distances are scaled by the cosine of the centre
	/// latitude so that the map is undistorted around its centre.
	class MapProjection {
	public:
		static constexpr double kDefaultPixelsPerDegree = 40.0;
		static constexpr double kMinPixelsPerDegree = 1.0;
		static constexpr double kMaxPixelsPerDegree = 100'000.0;

		MapProjection() noexcept = default;
		MapProjection(t_position center, double pixelsPerDegree, MapSize size) noexcept;

		/// The projection centred on `positions` that shows them all, `margin`
		/// pixels away from the edges at least, as far as the scale limits allow.
		/// An empty span yields the default view.
		[[nodiscard]] static MapProjection fitting(std::span<const t_position> positions, MapSize size, std::int32_t margin) noexcept;

		void setCenter(t_position center) noexcept;
		void setSize(MapSize size) noexcept { size_ = size; }

		[[nodiscard]] t_position center() const noexcept { return center_; }
		[[nodiscard]] double pixelsPerDegree() const noexcept { return pixelsPerDegree_; }
		[[nodiscard]] MapSize size() const noexcept { return size_; }

		/// The pixel of `position`; it lies outside the view when the position does.
		[[nodiscard]] MapPoint toPoint(t_position position) const noexcept;

		/// The position under `point`, clamped to the poles and wrapped in longitude.
		[[nodiscard]] t_position toPosition(MapPoint point) const noexcept;

	private:
		t_position center_{0, 0};
		double pixelsPerDegree_{kDefaultPixelsPerDegree};
		double cosCenterLatitude_{1.0};
		MapSize size_{0, 0};
	};

}
