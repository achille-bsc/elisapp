#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "ui/map/render/Color.hpp"

namespace mars::ui::render {

	/// RGB565 map picture written with a 4 x 4 ordered dither (no banding).
	class Canvas {
	public:
		Canvas(std::uint8_t* pixels, std::uint32_t width, std::uint32_t height, std::uint32_t stride) noexcept
			: pixels_{pixels}
			, width_{width}
			, height_{height}
			, stride_{stride}
		{
		}

		[[nodiscard]] std::uint32_t width() const noexcept { return width_; }
		[[nodiscard]] std::uint32_t height() const noexcept { return height_; }

		void set(std::uint32_t x, std::uint32_t y, Rgb c) noexcept
		{
			// Bayer matrix, thresholds 0..15
			static constexpr std::uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
			const std::uint32_t threshold = kBayer[y & 3U][x & 3U];

			const auto five = [threshold](std::uint8_t v) {
				return std::min<std::uint32_t>(31U, (std::uint32_t{v} + threshold / 2U) >> 3U);
			};
			const std::uint32_t six = std::min<std::uint32_t>(63U, (std::uint32_t{c.g} + threshold / 4U) >> 2U);
			const std::uint32_t v = five(c.r) << 11U | six << 5U | five(c.b);
			std::uint8_t* const at = pixels_ + std::size_t{y} * stride_ + std::size_t{x} * 2U;
			at[0] = static_cast<std::uint8_t>(v & 0xFFU);  // little endian
			at[1] = static_cast<std::uint8_t>(v >> 8U);
		}

		/// A line `width` pixels thick, clipped to the canvas.
		void line(float x0, float y0, float x1, float y1, Rgb c, int width) noexcept
		{
			const float length = std::fmax(std::fabs(x1 - x0), std::fabs(y1 - y0));
			const int steps = static_cast<int>(length) + 1;
			const int half = width / 2;
			for (int k = 0; k <= steps; ++k) {
				const float t = static_cast<float>(k) / static_cast<float>(steps);
				const int cx = static_cast<int>(std::lround(x0 + (x1 - x0) * t));
				const int cy = static_cast<int>(std::lround(y0 + (y1 - y0) * t));
				for (int dy = -half; dy < width - half; ++dy) {
					for (int dx = -half; dx < width - half; ++dx) {
						const int px = cx + dx;
						const int py = cy + dy;
						if (px >= 0 && py >= 0 && static_cast<std::uint32_t>(px) < width_ && static_cast<std::uint32_t>(py) < height_) {
							set(static_cast<std::uint32_t>(px), static_cast<std::uint32_t>(py), c);
						}
					}
				}
			}
		}

	private:
		std::uint8_t* pixels_;
		std::uint32_t width_;
		std::uint32_t height_;
		std::uint32_t stride_;
	};

}
