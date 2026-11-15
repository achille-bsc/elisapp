#pragma once

#include <array>
#include <cstdint>

#include "ui/map/MapLayer.hpp"

namespace mars::ui {

	/// Swatch fill; hatching keeps danger readable without colour (ISO 3864-1).
	enum class LegendPattern : std::uint8_t {
		Solid,
		Hatched,
	};

	struct LegendItem {
		std::uint32_t color;
		LegendPattern pattern;
		const char* label;
	};

	inline constexpr std::size_t kLegendMaxItems = 10;
	inline constexpr std::size_t kLegendGradientStops = 5;
	using LegendText = std::array<char, 160>;


	struct LegendSpec {
		const char* title{""};
		std::array<LegendItem, kLegendMaxItems> items{};
		std::uint8_t itemCount{0};
		/// Colour scale from `low` to `high`.
		bool hasGradient{false};
		std::array<std::uint32_t, kLegendGradientStops> gradient{};
		LegendText low{};
		LegendText high{};
		/// Sources and conditions, one or two lines.
		LegendText note{};
	};


	class IMapLegendSource {
	public:
		[[nodiscard]] virtual LegendSpec legend(MapLayer layer) const = 0;

	protected:
		~IMapLegendSource() = default;
	};

}
