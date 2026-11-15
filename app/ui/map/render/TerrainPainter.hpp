#pragma once

#include "ui/map/render/ILayerPainter.hpp"

namespace mars::ui::render {

	/// Orbital image shaded by the relief, or a hypsometric tint without image.
	class TerrainPainter final : public ILayerPainter {
	public:
		void paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas) override;
		[[nodiscard]] bool dependsOnTime() const noexcept override { return false; }
		[[nodiscard]] LegendSpec legend() const override { return legend_; }

	private:
		LegendSpec legend_{};
	};

}
