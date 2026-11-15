#include "core/mission/MissionJsonParser.hpp"

#include <cmath>
#include <cstring>

#include "core/gps/PositionFromDegrees.hpp"
#include "core/json/JsonReader.hpp"

namespace mars {

	namespace {

		constexpr double kMaxDurationMinutes = 65'535.0;
		constexpr double kMaxPercent = 100.0;

		[[nodiscard]] bool parseMission(json::Reader& reader, Mission& mission)
		{
			const char* name = nullptr;
			const char* type = "";
			const char* description = "";
			double latitude = NAN;
			double longitude = NAN;
			double duration = 0.0;
			double difficulty = 0.0;

			if (!reader.beginObject()) {
				return false;
			}
			const char* key = nullptr;
			while (reader.nextKey(key)) {
				bool ok = false;
				if (std::strcmp(key, "name") == 0) {
					ok = reader.readString(name);
				} else if (std::strcmp(key, "type") == 0) {
					ok = reader.readString(type);
				} else if (std::strcmp(key, "description") == 0) {
					ok = reader.readString(description);
				} else if (std::strcmp(key, "latitude") == 0) {
					ok = reader.readNumber(latitude);
				} else if (std::strcmp(key, "longitude") == 0) {
					ok = reader.readNumber(longitude);
				} else if (std::strcmp(key, "duration_minutes") == 0) {
					ok = reader.readNumber(duration);
				} else if (std::strcmp(key, "difficulty_percent") == 0) {
					ok = reader.readNumber(difficulty);
				} else {
					ok = reader.skipValue();
				}
				if (!ok) {
					return false;
				}
			}
			if (reader.failed() || name == nullptr || name[0] == '\0') {
				return false;
			}
			if (!(duration >= 0.0 && duration <= kMaxDurationMinutes) || !(difficulty >= 0.0 && difficulty <= kMaxPercent)) {
				return false;  // also rejects NaN
			}
			mission.name = name;
			mission.type = type;
			mission.description = description;
			mission.durationMinutes = static_cast<std::uint16_t>(std::lround(duration));
			mission.difficultyPercent = static_cast<std::uint8_t>(std::lround(difficulty));
			return positionFromDegrees(latitude, longitude, mission.destination);
		}

		[[nodiscard]] bool parseDocument(json::Reader& reader, std::vector<Mission>& missions)
		{
			bool found = false;
			if (!reader.beginObject()) {
				return false;
			}
			const char* key = nullptr;
			while (reader.nextKey(key)) {
				if (std::strcmp(key, "missions") != 0) {
					if (!reader.skipValue()) {
						return false;
					}
					continue;
				}
				if (found || !reader.beginArray()) {
					return false;
				}
				found = true;
				while (reader.nextElement()) {
					Mission mission{};
					if (!parseMission(reader, mission)) {
						return false;
					}
					missions.push_back(mission);
				}
			}
			return found && reader.atEnd();
		}

	}

	bool MissionJsonParser::parse(char* begin, char* end, std::vector<Mission>& missions)
	{
		missions.clear();
		json::Reader reader{begin, end};
		if (!parseDocument(reader, missions)) {
			missions.clear();
			return false;
		}
		return true;
	}

}
