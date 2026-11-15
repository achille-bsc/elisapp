#pragma once

#include <cstdint>
#include <vector>

#include "ui/map/render/ILayerPainter.hpp"

namespace mars::ui::render {

	/// Main reason a place is unsafe: one colour each.
	enum class RiskCategory : std::uint8_t {
		Safe,
		Uncertain,         ///< Slope read on coarse relief only.
		Steep,             ///< Caution: slope over 15 degrees.
		DustyGround,       ///< Caution: dusty ground (low thermal inertia).
		RadiationHigh,     ///< Caution: ALARA.
		TooSteep,          ///< Danger: slope over 20 degrees.
		SoftGround,        ///< Danger: soft ground, loose fines.
		Atmosphere,        ///< Danger: wind or dust storm.
		RadiationDanger,   ///< Danger: critical dose rate or solar storm.
		UnknownGround,     ///< Danger: no relief data.
	};

	inline constexpr std::size_t kRiskCategoryCount = 10;


	[[nodiscard]] RiskCategory riskCategoryOf(std::uint16_t reasons) noexcept;

	/// Hazard zones coloured by cause (Okabe-Ito), danger hatched and outlined.
	class RiskPainter final : public ILayerPainter {
	public:
		void paint(const ViewSamples& samples, std::uint32_t timeSeconds, Canvas& canvas) override;
		[[nodiscard]] bool dependsOnTime() const noexcept override { return true; }
		[[nodiscard]] LegendSpec legend() const override { return legend_; }

	private:
		LegendSpec legend_{};

		std::vector<RiskCategory> grid_;
	};

}
