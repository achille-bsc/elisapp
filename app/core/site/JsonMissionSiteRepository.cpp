#include "core/site/JsonMissionSiteRepository.hpp"

#include <cmath>
#include <cstring>

#include "core/gps/PositionFromDegrees.hpp"
#include "core/json/JsonReader.hpp"
#include "core/json/TextFile.hpp"

namespace mars {

	namespace {

		constexpr double kHoursPerSol = 24.0;
		constexpr double kFullTurnDegrees = 360.0;
		constexpr double kMetresPerKm = 1'000.0;
		/// A first view wider than this is no longer "around the base".
		constexpr double kMaxViewRadiusKm = 2'000.0;

		[[nodiscard]] bool parseSite(json::Reader& reader, MissionSite& site)
		{
			const char* name = nullptr;
			const char* description = "";
			double latitude = NAN;
			double longitude = NAN;
			double season = 0.0;
			double hour = site.startLocalHour;
			double viewKm = site.viewRadiusM / kMetresPerKm;

			if (!reader.beginObject()) {
				return false;
			}
			const char* key = nullptr;
			while (reader.nextKey(key)) {
				bool ok = false;
				if (std::strcmp(key, "name") == 0) {
					ok = reader.readString(name);
				} else if (std::strcmp(key, "description") == 0) {
					ok = reader.readString(description);
				} else if (std::strcmp(key, "latitude") == 0) {
					ok = reader.readNumber(latitude);
				} else if (std::strcmp(key, "longitude") == 0) {
					ok = reader.readNumber(longitude);
				} else if (std::strcmp(key, "solar_longitude_deg") == 0) {
					ok = reader.readNumber(season);
				} else if (std::strcmp(key, "start_local_hour") == 0) {
					ok = reader.readNumber(hour);
				} else if (std::strcmp(key, "view_radius_km") == 0) {
					ok = reader.readNumber(viewKm);
				} else {
					ok = reader.skipValue();  // zones of the data tool
				}
				if (!ok) {
					return false;
				}
			}
			if (reader.failed() || name == nullptr || name[0] == '\0' || !(season >= 0.0 && season < kFullTurnDegrees)
				|| !(hour >= 0.0 && hour < kHoursPerSol) || !(viewKm > 0.0 && viewKm <= kMaxViewRadiusKm)) {
				return false;
			}
			MissionSite parsed = site;
			if (!positionFromDegrees(latitude, longitude, parsed.position)) {
				return false;
			}
			parsed.name = name;
			parsed.description = description;
			parsed.solarLongitudeDegrees = season;
			parsed.startLocalHour = static_cast<std::uint32_t>(hour);
			parsed.viewRadiusM = static_cast<std::uint32_t>(std::lround(viewKm * kMetresPerKm));
			site = parsed;
			return true;
		}

	}

	bool JsonMissionSiteRepository::parse(char* begin, char* end, MissionSite& site)
	{
		json::Reader reader{begin, end};
		MissionSite parsed = site;
		bool found = false;
		if (!reader.beginObject()) {
			return false;
		}
		const char* key = nullptr;
		while (reader.nextKey(key)) {
			if (std::strcmp(key, "mission_site") != 0) {
				if (!reader.skipValue()) {
					return false;
				}
				continue;
			}
			if (found || !parseSite(reader, parsed)) {
				return false;
			}
			found = true;
		}
		if (!found || !reader.atEnd()) {
			return false;
		}
		site = parsed;
		return true;
	}

	bool JsonMissionSiteRepository::load(const char* path)
	{
		auto text = std::make_unique<std::string>();
		MissionSite parsed = site_;
		if (!json::readTextFile(path, kMaxFileBytes, *text) || !parse(text->data(), text->data() + text->size(), parsed)) {
			return false;
		}
		// The parsed strings point into the string itself: only its owner moves.
		text_ = std::move(text);
		site_ = parsed;
		return true;
	}

}
