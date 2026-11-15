#pragma once

#include <vector>

#include "ui/map/render/ILayerPainter.hpp"

namespace mars::ui::render {

	/// Air temperature with isotherms and wind arrows.
	class WeatherPainter final : public ILayerPainter {
	public:
		/// `source` (legend text) must outlive the painter.
		explicit WeatherPainter(const char* source) noexcept : source_{source} {}

		void paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas) override;
		[[nodiscard]] bool dependsOnTime() const noexcept override { return true; }
		[[nodiscard]] LegendSpec legend() const override { return legend_; }

	private:
		const char* source_;
		LegendSpec legend_{};
		/// Kelvin on a coarse grid, NaN without data.
		std::vector<float> grid_;
	};

}
