#pragma once

#include <cstdint>

#include "core/gps/Position.hpp"

namespace mars {

	/// The crew's base (assets/mission_site.json): first view and data zones.
	struct MissionSite {
		const char* name;
		const char* description;
		t_position position;
		/// Mars solar longitude Ls, degrees.
		double solarLongitudeDegrees;
		std::uint32_t startLocalHour;
		/// Half the width of ground shown by the first map, in metres.
		std::uint32_t viewRadiusM;
	};

	class IMissionSiteRepository {
	public:
		IMissionSiteRepository() = default;
		IMissionSiteRepository(const IMissionSiteRepository&) = delete;
		IMissionSiteRepository& operator=(const IMissionSiteRepository&) = delete;
		virtual ~IMissionSiteRepository() = default;

		[[nodiscard]] virtual const MissionSite& site() const noexcept = 0;
	};

}
