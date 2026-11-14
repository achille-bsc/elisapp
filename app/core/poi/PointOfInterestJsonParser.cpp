#include "core/poi/PointOfInterestJsonParser.hpp"

#include <cmath>
#include <cstring>

#include "core/gps/PositionFromDegrees.hpp"
#include "core/json/JsonReader.hpp"

namespace mars {

	namespace {

		[[nodiscard]] bool parsePoint(json::Reader& reader, PointOfInterest& point)
		{
			const char* name = nullptr;
			const char* description = "";
			double latitude = NAN;
			double longitude = NAN;

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
			point.name = name;
			point.description = description;
			return positionFromDegrees(latitude, longitude, point.position);
		}

		[[nodiscard]] bool parseDocument(json::Reader& reader, std::vector<PointOfInterest>& points)
		{
			bool found = false;
			if (!reader.beginObject()) {
				return false;
			}
			const char* key = nullptr;
			while (reader.nextKey(key)) {
				if (std::strcmp(key, "points_of_interest") != 0) {
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
					PointOfInterest point{};
					if (!parsePoint(reader, point)) {
						return false;
					}
					points.push_back(point);
				}
			}
			return found && reader.atEnd();
		}

	}

	bool PointOfInterestJsonParser::parse(char* begin, char* end, std::vector<PointOfInterest>& points)
	{
		points.clear();
		json::Reader reader{begin, end};
		if (!parseDocument(reader, points)) {
			points.clear();
			return false;
		}
		return true;
	}

}
