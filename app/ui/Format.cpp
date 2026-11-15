#include "ui/Format.hpp"

#include <cstdio>
#include <cstring>

namespace mars::ui {

	namespace {

		constexpr std::uint32_t kMetresPerKm = 1'000;
		constexpr std::uint32_t kSecondsPerMinute = 60;
		constexpr std::uint32_t kMinutesPerHour = 60;
		constexpr std::uint32_t kHoursPerDay = 24;

		struct ReasonName {
			HazardReason reason;
			const char* name;
		};

		constexpr ReasonName kReasonNames[] = {
			{HazardReason::SolarParticleEvent, "solar storm: seek shelter"},
			{HazardReason::NoTerrain, "unknown terrain"},
			{HazardReason::TooSteep, "slope over 20°"},
			{HazardReason::DustStorm, "dust storm"},
			{HazardReason::Wind, "strong wind"},
			{HazardReason::RadiationCritical, "critical radiation"},
			{HazardReason::DoseBudget, "radiation dose over the career limit"},
			{HazardReason::SoftGround, "soft ground"},
			{HazardReason::Steep, "slope over 15°"},
			{HazardReason::DustyGround, "dusty ground"},
			{HazardReason::RadiationHigh, "high radiation"},
			{HazardReason::EvaTooLong, "longer than one EVA (8 h)"},
			{HazardReason::BeyondZone, "over 100 km from the start"},
			{HazardReason::CoarseTerrain, "slope from coarse relief only"},
			{HazardReason::NoWeather, "no weather data"},
		};

	}

	ShortText formatDistance(std::uint32_t metres) noexcept
	{
		ShortText text{};
		if (metres < kMetresPerKm) {
			std::snprintf(text.data(), text.size(), "%u m", static_cast<unsigned>(metres));
		} else {
			// One decimal: hundreds of metres.
			std::snprintf(text.data(), text.size(), "%u.%u km", static_cast<unsigned>(metres / kMetresPerKm),
				static_cast<unsigned>(metres % kMetresPerKm / 100));
		}
		return text;
	}

	ShortText formatDuration(std::uint32_t seconds) noexcept
	{
		const std::uint32_t minutes = (seconds + kSecondsPerMinute / 2) / kSecondsPerMinute;
		const std::uint32_t hours = minutes / kMinutesPerHour;
		ShortText text{};
		if (hours == 0) {
			std::snprintf(text.data(), text.size(), "%u min", static_cast<unsigned>(minutes));
		} else if (hours < kHoursPerDay) {
			std::snprintf(text.data(), text.size(), "%u h %02u min", static_cast<unsigned>(hours),
				static_cast<unsigned>(minutes % kMinutesPerHour));
		} else {
			std::snprintf(text.data(), text.size(), "%u d %u h", static_cast<unsigned>(hours / kHoursPerDay),
				static_cast<unsigned>(hours % kHoursPerDay));
		}
		return text;
	}

	const char* hazardLevelName(HazardLevel level) noexcept
	{
		switch (level) {
		case HazardLevel::Safe:
			return "Safe";
		case HazardLevel::Caution:
			return "Caution";
		default:
			return "Danger";
		}
	}

	LongText formatHazardReasons(HazardReasons reasons) noexcept
	{
		LongText text{};
		std::size_t used = 0;
		for (const ReasonName& entry : kReasonNames) {
			if (!hasReason(reasons, entry.reason)) {
				continue;
			}
			const int written = std::snprintf(text.data() + used, text.size() - used, "%s%s", used == 0 ? "" : ", ", entry.name);
			if (written < 0 || static_cast<std::size_t>(written) >= text.size() - used) {
				break;  // full: the most severe reasons come first and are kept
			}
			used += static_cast<std::size_t>(written);
		}
		return text;
	}

	const char* routeStatusMessage(RouteStatus status) noexcept
	{
		switch (status) {
		case RouteStatus::Ok:
			return "";
		case RouteStatus::StartInHazard:
			return "No route: you are standing in a danger zone.";
		case RouteStatus::GoalInHazard:
			return "No route: the destination is in a danger zone.";
		case RouteStatus::NoPath:
			return "No route: every way there crosses a danger zone.";
		case RouteStatus::TooFar:
			return "No route: the destination is out of range.";
		default:
			return "No route: planning failed.";
		}
	}

}
