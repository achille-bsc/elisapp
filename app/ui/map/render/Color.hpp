#pragma once

#include <algorithm>
#include <cstdint>

namespace mars::ui::render {


	struct Rgb {
		std::uint8_t r;
		std::uint8_t g;
		std::uint8_t b;
	};

	inline constexpr std::uint32_t kChannelMax = 255;

	[[nodiscard]] constexpr Rgb rgb(std::uint32_t hex) noexcept
	{
		return {static_cast<std::uint8_t>(hex >> 16U & kChannelMax), static_cast<std::uint8_t>(hex >> 8U & kChannelMax),
			static_cast<std::uint8_t>(hex & kChannelMax)};
	}

	[[nodiscard]] constexpr std::uint32_t hexOf(Rgb c) noexcept
	{
		return std::uint32_t{c.r} << 16U | std::uint32_t{c.g} << 8U | c.b;
	}

	[[nodiscard]] inline std::uint8_t channel(float v) noexcept
	{
		return static_cast<std::uint8_t>(std::clamp(v + 0.5F, 0.0F, static_cast<float>(kChannelMax)));
	}

	/// a when t = 0, b when t = 1.
	[[nodiscard]] inline Rgb lerp(Rgb a, Rgb b, float t) noexcept
	{
		const auto mix = [t](std::uint8_t from, std::uint8_t to) {
			return channel(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t);
		};
		return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
	}


	[[nodiscard]] inline Rgb scale(Rgb c, float k) noexcept
	{
		return {channel(c.r * k), channel(c.g * k), channel(c.b * k)};
	}

	/// `over` on `base` with opacity `alpha` in [0, 1].
	[[nodiscard]] inline Rgb blend(Rgb base, Rgb over, float alpha) noexcept
	{
		return lerp(base, over, alpha);
	}

	/// Ramp through evenly spread `stops`, t in [0, 1].
	template <std::size_t N>
	[[nodiscard]] Rgb ramp(const Rgb (&stops)[N], float t) noexcept
	{
		static_assert(N >= 2);
		const float at = std::clamp(t, 0.0F, 1.0F) * static_cast<float>(N - 1);
		const auto index = std::min(static_cast<std::size_t>(at), N - 2);
		return lerp(stops[index], stops[index + 1], at - static_cast<float>(index));
	}

}
