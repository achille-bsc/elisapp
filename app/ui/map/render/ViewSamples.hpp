#pragma once

#include <cstdint>
#include <vector>

#include "core/map/MapProjection.hpp"
#include "data/core/coord.hpp"

namespace mars::ui::render {

	/// Relief and image under every pixel, sampled once per view for all layers.
	class ViewSamples {
	public:
		static constexpr std::uint16_t kNoGrey = 0xFFFF;

		/// True if the view changed and was sampled again.
		bool update(const MapProjection& projection);

		[[nodiscard]] std::uint32_t width() const noexcept { return width_; }
		[[nodiscard]] std::uint32_t height() const noexcept { return height_; }
		[[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const noexcept
		{
			return std::size_t{y} * width_ + x;
		}


		[[nodiscard]] t_coord coord(std::uint32_t x, std::uint32_t y) const noexcept
		{
			return {lonUdeg_[x], latUdeg_[y]};
		}

		/// Metres above the MOLA areoid, NaN where unknown.
		[[nodiscard]] float elevation(std::size_t i) const noexcept { return elevation_[i]; }

		/// Shading lit from the north-west, 1 on flat ground.
		[[nodiscard]] float shade(std::size_t i) const noexcept { return shade_[i]; }

		/// Image grey in 1/256 steps, kNoGrey where none.
		[[nodiscard]] std::uint16_t grey(std::size_t i) const noexcept { return grey_[i]; }

		[[nodiscard]] double metresPerPixel() const noexcept { return metresPerPixel_; }
		[[nodiscard]] float lowest() const noexcept { return lowest_; }
		[[nodiscard]] float highest() const noexcept { return highest_; }
		[[nodiscard]] bool hasImagery() const noexcept { return hasImagery_; }

	private:
		void shadeRelief();

		std::uint32_t width_{0};
		std::uint32_t height_{0};
		t_position center_{0, 0};
		double pixelsPerDegree_{0.0};
		double metresPerPixel_{1.0};
		float lowest_{0.0F};
		float highest_{0.0F};
		bool hasImagery_{false};
		std::vector<std::uint32_t> lonUdeg_;
		std::vector<std::int32_t> latUdeg_;
		std::vector<float> elevation_;
		std::vector<float> shade_;
		std::vector<std::uint16_t> grey_;
	};

}
