#pragma once

#include <span>

#include "core/poi/PointOfInterest.hpp"

namespace mars {

	/// Read access to the points of interest, wherever they come from.
	class IPointOfInterestRepository {
	public:
		IPointOfInterestRepository() = default;
		IPointOfInterestRepository(const IPointOfInterestRepository&) = delete;
		IPointOfInterestRepository& operator=(const IPointOfInterestRepository&) = delete;
		virtual ~IPointOfInterestRepository() = default;

		/// Valid until the repository is reloaded or destroyed.
		[[nodiscard]] virtual std::span<const PointOfInterest> all() const noexcept = 0;
	};

}
