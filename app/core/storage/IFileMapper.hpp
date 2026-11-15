#pragma once

#include <cstdint>

namespace mars {

	/// Read-only file mapping, valid as long as the mapper.
	class IFileMapper {
	public:
		[[nodiscard]] virtual const void* mapFile(const char* path, std::uint32_t& size) = 0;

	protected:
		~IFileMapper() = default;
	};

}
