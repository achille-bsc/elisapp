#pragma once

#include <vector>

#include "ui/map/render/ILayerPainter.hpp"

namespace mars::ui::render {

	/// Dose rate on the ground with isolines; NASA-STD-3001 limits in bold.
	class RadiationPainter final : public ILayerPainter {
	public:
		void paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas) override;
		[[nodiscard]] bool dependsOnTime() const noexcept override { return true; }
		[[nodiscard]] LegendSpec legend() const override { return legend_; }

	private:
		LegendSpec legend_{};
		/// uSv/sol on a coarse grid, NaN without data.
		std::vector<float> grid_;
	};

}
