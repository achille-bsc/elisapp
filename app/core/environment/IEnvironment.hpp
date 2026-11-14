#pragma once

#include <cstdint>

#include "core/environment/Hazard.hpp"
#include "core/gps/Position.hpp"

namespace mars {

	/// Everything known about one place at one time. Values keep the fixed-point
	/// units of the data module; a group is meaningful only when its flag is set.
	struct EnvironmentSample {
		std::int16_t elevationM;
		std::uint8_t slopeHalfDegrees;
		bool hasTerrain;

		std::uint16_t temperatureDeciKelvin;
		std::uint16_t pressureDeciPascal;
		std::uint16_t windCmPerSecond;
		std::uint8_t dustOpacityX40;
		bool hasWeather;

		std::uint16_t doseMicroSievertPerSol;
		bool hasRadiation;

		HazardLevel level;
		HazardReasons reasons;
	};

	/// Read access to the relief, the local weather, the radiation and the
	/// hazard assessment of any place on Mars.
	class IEnvironment {
	public:
		IEnvironment() = default;
		IEnvironment(const IEnvironment&) = delete;
		IEnvironment& operator=(const IEnvironment&) = delete;
		virtual ~IEnvironment() = default;

		[[nodiscard]] virtual EnvironmentSample sample(t_position position, std::uint32_t timeSeconds) const noexcept = 0;
	};

}
