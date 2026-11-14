#pragma once

#include <string>
#include <vector>

#include "core/poi/IPointOfInterestRepository.hpp"

namespace mars {

	/// Points of interest read from a JSON file. The file text is kept as the single
	/// backing store of every name and description (one allocation for all strings).
	class JsonPointOfInterestRepository final : public IPointOfInterestRepository {
	public:
		/// Replaces the current points with those of `path`. On failure (missing or
		/// malformed file) the repository is left empty and false is returned.
		[[nodiscard]] bool load(const char* path);

		[[nodiscard]] std::span<const PointOfInterest> all() const noexcept override { return points_; }

	private:
		/// Upper bound on the file size, to keep memory use predictable.
		static constexpr long kMaxFileBytes = 256L * 1024L;

		std::string text_;
		std::vector<PointOfInterest> points_;
	};

}
