#include "integration/DataEnvironment.hpp"

#include "data/data.hpp"
#include "integration/DataUnits.hpp"

namespace mars {

	EnvironmentSample DataEnvironment::sample(t_position position, std::uint32_t timeSeconds) const noexcept
	{
		const t_coord coord = toCoord(position);
		const t_data_point data = data_gathering(coord, timeSeconds, DATA_ALL);

		const t_hazard hazard = hazard_assess(coord, timeSeconds);

		EnvironmentSample sample{};
		sample.elevationM = data.landform.elevation_m;
		sample.slopeHalfDegrees = data.landform.slope_half_deg;
		sample.hasTerrain = data.landform.valid;
		sample.temperatureDeciKelvin = data.weather.temperature_dk;
		sample.pressureDeciPascal = data.weather.pressure_dpa;
		sample.windCmPerSecond = data.weather.wind_cms;
		sample.dustOpacityX40 = data.weather.dust_x40;
		sample.windFromX256 = data.weather.wind_from_x256;
		sample.hasWeather = data.weather.valid;
		sample.doseMicroSievertPerSol = data.radiation.dose_eq_usv_day;
		sample.hasRadiation = data.radiation.valid;
		sample.level = toHazardLevel(hazard.level);
		sample.reasons = hazard.reasons;
		return sample;
	}

}
