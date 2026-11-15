#pragma once

#include <memory>
#include <string>

#include "core/site/MissionSite.hpp"

namespace mars {

	/// The mission site read from a JSON file (see assets/mission_site.json).
	class JsonMissionSiteRepository final : public IMissionSiteRepository {
	public:
		/// `fallback`'s strings must outlive the repository.
		explicit JsonMissionSiteRepository(const MissionSite& fallback) noexcept : site_{fallback} {}

		/// On failure the site is unchanged.
		[[nodiscard]] bool load(const char* path);

		/// Parses a document in place (`end` must be writable, see json::Reader).
		[[nodiscard]] static bool parse(char* begin, char* end, MissionSite& site);

		[[nodiscard]] const MissionSite& site() const noexcept override { return site_; }

	private:
		static constexpr long kMaxFileBytes = 16L * 1024L;

		/// Behind a pointer: the site strings point into it.
		std::unique_ptr<std::string> text_;
		MissionSite site_;
	};

}
