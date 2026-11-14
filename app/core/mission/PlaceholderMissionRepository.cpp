#include "core/mission/PlaceholderMissionRepository.hpp"

#include <array>

namespace mars {

	namespace {

		constexpr const char* kDescription =
			"This is a mission description: Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
			"eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
			"exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.";

		constexpr Mission placeholder(std::int32_t latitudeE7, std::uint32_t longitudeEastE7) noexcept
		{
			return {"Place holder mission", "Geology", kDescription, {latitudeE7, longitudeEastE7}, 30, 45, 20, 60};
		}

		constexpr std::array<Mission, 5> kMissions{
			placeholder(184'447'000, 774'508'000),
			placeholder(99'000'000, 700'000'000),
			placeholder(129'000'000, 870'000'000),
			placeholder(115'300'000, 904'300'000),
			placeholder(220'000'000, 760'000'000),
		};

	}

	std::span<const Mission> PlaceholderMissionRepository::all() const noexcept
	{
		return kMissions;
	}

}
