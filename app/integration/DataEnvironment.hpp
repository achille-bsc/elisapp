#pragma once

#include "core/environment/IEnvironment.hpp"

namespace mars {

	/// IEnvironment served by the data module (app/data). A DataSession must be
	/// alive while it is used.
	class DataEnvironment final : public IEnvironment {
	public:
		[[nodiscard]] EnvironmentSample sample(t_position position, std::uint32_t timeSeconds) const noexcept override;
	};

}
