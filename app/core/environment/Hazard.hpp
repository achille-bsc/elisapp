#pragma once

#include <cstdint>

namespace mars {

	/// How dangerous a place or a route is for a crew on foot.
	enum class HazardLevel : std::uint8_t {
		Safe,
		Caution,
		Danger,
	};

	/// Why a place or a route is not safe. Bit flags, combined in HazardReasons.
	/// The limits behind each of them come from the NASA standards listed in
	/// SOURCES.txt (see app/data/config.hpp).
	enum class HazardReason : std::uint16_t {
		NoTerrain = 1U << 0U,          ///< Danger: outside the known relief.
		CoarseTerrain = 1U << 1U,      ///< Caution: slope known from coarse data only.
		Steep = 1U << 2U,              ///< Caution: slope above 15 degrees.
		TooSteep = 1U << 3U,           ///< Danger: slope above 20 degrees.
		Wind = 1U << 4U,               ///< Danger.
		DustStorm = 1U << 5U,          ///< Danger.
		RadiationHigh = 1U << 6U,      ///< Caution (ALARA).
		RadiationCritical = 1U << 7U,  ///< Danger: career dose limit.
		SolarParticleEvent = 1U << 8U, ///< Danger: seek shelter.
		NoWeather = 1U << 9U,          ///< Information: no satellite data.
		EvaTooLong = 1U << 10U,        ///< Caution: more than one EVA on foot.
		BeyondZone = 1U << 11U,        ///< Caution: outside the exploration zone.
		DoseBudget = 1U << 12U,        ///< Danger: career dose exceeded on the way.
		SoftGround = 1U << 13U,        ///< Danger: loose fines, low thermal inertia.
		DustyGround = 1U << 14U,       ///< Caution: dusty ground.
	};

	using HazardReasons = std::uint16_t;

	[[nodiscard]] constexpr bool hasReason(HazardReasons reasons, HazardReason reason) noexcept
	{
		return (reasons & static_cast<HazardReasons>(reason)) != 0U;
	}

}
