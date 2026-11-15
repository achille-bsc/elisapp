#include "integration/DataStormForecast.hpp"

#include <cmath>
#include <cstring>

#include "core/gps/PositionFromDegrees.hpp"
#include "core/json/JsonReader.hpp"
#include "core/json/TextFile.hpp"
#include "data/data.hpp"

namespace mars {

	namespace {

		constexpr double kMetresPerKm = 1'000.0;
		constexpr double kMmPerM = 1'000.0;
		/// Dust opacity and wind as the data module stores them.
		constexpr double kDustScale = 40.0;
		constexpr double kCmPerM = 100.0;

		struct Parsed {
			const char* name{nullptr};
			double latitude{NAN};
			double longitude{NAN};
			double radiusKm{0.0};
			double startHours{0.0};
			double durationHours{0.0};
			double driftEast{0.0};
			double driftNorth{0.0};
			double dust{0.0};
			double wind{0.0};
		};

		[[nodiscard]] bool parseStorm(json::Reader& reader, Parsed& storm)
		{
			if (!reader.beginObject()) {
				return false;
			}
			const char* key = nullptr;
			while (reader.nextKey(key)) {
				const struct {
					const char* key;
					double* value;
				} numbers[] = {
					{"latitude", &storm.latitude}, {"longitude", &storm.longitude}, {"radius_km", &storm.radiusKm},
					{"start_in_hours", &storm.startHours}, {"duration_hours", &storm.durationHours},
					{"drift_east_m_s", &storm.driftEast}, {"drift_north_m_s", &storm.driftNorth},
					{"dust_opacity", &storm.dust}, {"wind_m_s", &storm.wind},
				};
				bool ok = false;
				bool known = false;
				if (std::strcmp(key, "name") == 0) {
					ok = reader.readString(storm.name);
					known = true;
				}
				for (const auto& number : numbers) {
					if (!known && std::strcmp(key, number.key) == 0) {
						ok = reader.readNumber(*number.value);
						known = true;
					}
				}
				if (!known) {
					ok = reader.skipValue();
				}
				if (!ok) {
					return false;
				}
			}
			return storm.name != nullptr && storm.radiusKm > 0.0 && storm.startHours >= 0.0
				&& storm.durationHours > 0.0 && storm.dust >= 0.0 && storm.wind >= 0.0;
		}

		/// mm/s, clamped before the conversion (out of range: refused later).
		[[nodiscard]] std::int32_t driftMmS(double metresPerSecond) noexcept
		{
			constexpr double kLimit = HAZARD_STORM_MAX_DRIFT_MM_S + 1.0;
			return static_cast<std::int32_t>(std::lround(std::fmin(std::fmax(metresPerSecond * kMmPerM, -kLimit), kLimit)));
		}

		[[nodiscard]] std::uint32_t clampU32(double v) noexcept
		{
			return static_cast<std::uint32_t>(std::lround(std::fmin(std::fmax(v, 0.0), 4.0e9)));
		}

	}

	bool DataStormForecast::load(const char* path, std::uint32_t startSeconds)
	{
		auto text = std::make_unique<std::string>();
		static_cast<void>(hazard_set_storms(nullptr, 0));
		names_.clear();
		if (!json::readTextFile(path, kMaxFileBytes, *text)) {
			return false;
		}
		json::Reader reader{text->data(), text->data() + text->size()};
		std::vector<t_storm> storms;
		std::vector<const char*> names;
		const char* source = nullptr;
		if (!reader.beginObject()) {
			return false;
		}
		const char* key = nullptr;
		while (reader.nextKey(key)) {
			bool ok = false;
			if (std::strcmp(key, "source") == 0) {
				ok = reader.readString(source);
			} else if (std::strcmp(key, "storms") == 0 && reader.beginArray()) {
				ok = true;
				while (ok && reader.nextElement()) {
					Parsed p;
					t_position centre{};
					ok = parseStorm(reader, p) && positionFromDegrees(p.latitude, p.longitude, centre);
					if (!ok) {
						break;
					}
					const std::uint32_t start = startSeconds + clampU32(p.startHours * MARS_HOUR_S);
					t_storm storm{};
					storm.centre = coord_from_e7_east(centre.latitudeE7, centre.longitudeEastE7);
					storm.drift_east_mm_s = driftMmS(p.driftEast);
					storm.drift_north_mm_s = driftMmS(p.driftNorth);
					storm.radius_mm = clampU32(p.radiusKm * kMetresPerKm * kMmPerM);
					storm.start_s = start;
					storm.end_s = start + clampU32(p.durationHours * MARS_HOUR_S);
					storm.wind_cms = static_cast<std::uint16_t>(std::fmin(p.wind * kCmPerM, UINT16_MAX));
					storm.dust_x40 = static_cast<std::uint8_t>(std::fmin(p.dust * kDustScale, UINT8_MAX));
					storms.push_back(storm);
					names.push_back(p.name);
				}
			} else {
				ok = reader.skipValue();
			}
			if (!ok) {
				return false;
			}
		}
		if (!reader.atEnd() || storms.size() > HAZARD_MAX_STORMS
			|| !hazard_set_storms(storms.data(), static_cast<std::uint8_t>(storms.size()))) {
			return false;
		}
		text_ = std::move(text);
		names_ = std::move(names);
		source_ = source != nullptr ? source : "Storm forecast.";
		return true;
	}

	std::span<const StormView> DataStormForecast::at(std::uint32_t timeSeconds)
	{
		views_.clear();
		for (std::uint8_t i = 0; i < hazard_storm_count() && i < names_.size(); ++i) {
			const t_storm& storm = *hazard_storm(i);
			if (timeSeconds >= storm.end_s) {
				continue;
			}
			const t_coord centre = hazard_storm_centre(storm, timeSeconds);
			const t_position position{coord_lat_e7(centre), static_cast<std::uint32_t>(std::uint64_t{centre.lon_udeg} * E7_PER_UDEG)};
			views_.push_back({names_[i], position, static_cast<std::uint32_t>(storm.radius_mm / kMmPerM), storm.start_s,
				storm.end_s, timeSeconds >= storm.start_s});
		}
		return views_;
	}

}
