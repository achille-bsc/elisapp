#pragma once

#include <string>
#include <vector>

#include "core/mission/IMissionRepository.hpp"

namespace mars {

	/// Missions read from a JSON file. The file text is kept as the single
	/// backing store of every string (one allocation for all of them).
	class JsonMissionRepository final : public IMissionRepository {
	public:
		/// Replaces the current missions with those of `path`. On failure (missing
		/// or malformed file) the repository is left empty and false is returned.
		[[nodiscard]] bool load(const char* path);

		[[nodiscard]] std::span<const Mission> all() const noexcept override { return missions_; }

	private:
		/// Upper bound on the file size, to keep memory use predictable.
		static constexpr long kMaxFileBytes = 256L * 1024L;

		std::string text_;
		std::vector<Mission> missions_;
	};

}
