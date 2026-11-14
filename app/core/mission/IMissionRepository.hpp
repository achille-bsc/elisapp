#pragma once

#include <span>

#include "core/mission/Mission.hpp"

namespace mars {

	/// Read access to the missions, wherever they come from.
	class IMissionRepository {
	public:
		IMissionRepository() = default;
		IMissionRepository(const IMissionRepository&) = delete;
		IMissionRepository& operator=(const IMissionRepository&) = delete;
		virtual ~IMissionRepository() = default;

		/// Valid until the repository is reloaded or destroyed.
		[[nodiscard]] virtual std::span<const Mission> all() const noexcept = 0;
	};

}
