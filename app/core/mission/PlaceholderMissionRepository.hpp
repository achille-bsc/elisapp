#pragma once

#include "core/mission/IMissionRepository.hpp"

namespace mars {

	/// Fixed placeholder missions, to be replaced by a repository backed by real data.
	class PlaceholderMissionRepository final : public IMissionRepository {
	public:
		[[nodiscard]] std::span<const Mission> all() const noexcept override;
	};

}
