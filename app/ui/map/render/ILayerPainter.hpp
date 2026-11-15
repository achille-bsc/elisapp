#pragma once

#include <cstdint>

#include "ui/map/render/Canvas.hpp"
#include "ui/map/render/Legend.hpp"
#include "ui/map/render/ViewSamples.hpp"

namespace mars::ui::render {

	/// Draws one map layer from the view samples.
	class ILayerPainter {
	public:
		ILayerPainter() = default;
		ILayerPainter(const ILayerPainter&) = delete;
		ILayerPainter& operator=(const ILayerPainter&) = delete;
		virtual ~ILayerPainter() = default;


		virtual void paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas) = 0;

		/// True if the picture changes with the mission time.
		[[nodiscard]] virtual bool dependsOnTime() const noexcept = 0;


		[[nodiscard]] virtual LegendSpec legend() const = 0;
	};

	/// Diagonal hatching over danger and uncertain areas.
	inline constexpr std::uint32_t kHatchPeriod = 8;
	inline constexpr std::uint32_t kHatchWidth = 2;

	[[nodiscard]] constexpr bool onHatch(std::uint32_t x, std::uint32_t y) noexcept
	{
		return (x + y) % kHatchPeriod < kHatchWidth;
	}

	/// Fill where there is no data.
	inline constexpr Rgb kNoData = rgb(0x1E1410);
	inline constexpr Rgb kNoDataHatch = rgb(0x4A3A32);

}
