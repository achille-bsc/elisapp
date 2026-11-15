#pragma once

#include "core/environment/Hazard.hpp"
#include "core/gps/Position.hpp"
#include "data/data.hpp"

// Conversions at the border between the application types and the data module.
namespace mars {

	[[nodiscard]] constexpr t_coord toCoord(t_position position) noexcept
	{
		return coord_from_e7_east(position.latitudeE7, position.longitudeEastE7);
	}

	[[nodiscard]] constexpr HazardLevel toHazardLevel(std::uint8_t level) noexcept
	{
		switch (level) {
		case HAZARD_SAFE:
			return HazardLevel::Safe;
		case HAZARD_CAUTION:
			return HazardLevel::Caution;
		default:
			return HazardLevel::Danger;
		}
	}

	// HazardReason mirrors the bits of the data module: no translation needed.
	static_assert(static_cast<HazardReasons>(HazardReason::NoTerrain) == HAZARD_NO_TERRAIN);
	static_assert(static_cast<HazardReasons>(HazardReason::CoarseTerrain) == HAZARD_COARSE_TERRAIN);
	static_assert(static_cast<HazardReasons>(HazardReason::Steep) == HAZARD_STEEP);
	static_assert(static_cast<HazardReasons>(HazardReason::TooSteep) == HAZARD_TOO_STEEP);
	static_assert(static_cast<HazardReasons>(HazardReason::Wind) == HAZARD_WIND);
	static_assert(static_cast<HazardReasons>(HazardReason::DustStorm) == HAZARD_DUST_STORM);
	static_assert(static_cast<HazardReasons>(HazardReason::RadiationHigh) == HAZARD_RADIATION_HIGH);
	static_assert(static_cast<HazardReasons>(HazardReason::RadiationCritical) == HAZARD_RADIATION_CRITICAL);
	static_assert(static_cast<HazardReasons>(HazardReason::SolarParticleEvent) == HAZARD_SPE_ALERT);
	static_assert(static_cast<HazardReasons>(HazardReason::NoWeather) == HAZARD_NO_WEATHER);
	static_assert(static_cast<HazardReasons>(HazardReason::EvaTooLong) == HAZARD_EVA_TOO_LONG);
	static_assert(static_cast<HazardReasons>(HazardReason::BeyondZone) == HAZARD_BEYOND_ZONE);
	static_assert(static_cast<HazardReasons>(HazardReason::DoseBudget) == HAZARD_DOSE_BUDGET);
	static_assert(static_cast<HazardReasons>(HazardReason::SoftGround) == HAZARD_SOFT_GROUND);
	static_assert(static_cast<HazardReasons>(HazardReason::DustyGround) == HAZARD_DUSTY_GROUND);

}
