// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace marspath {

/// Non-fatal conditions, combinable as bit flags.
enum class Warning : std::uint8_t {
    None = 0,
    AscentLimitExceeded = 1U << 0U,
    DescentLimitExceeded = 1U << 1U,
    ConstraintUnreachable = 1U << 2U,     ///< No coarse route meets the elevation limits.
    NetElevationExceedsLimit = 1U << 3U,  ///< Start/goal altitude difference alone exceeds a limit.
    TimeBudgetReached = 1U << 4U,
};

[[nodiscard]] constexpr Warning operator|(Warning a, Warning b) noexcept {
    return static_cast<Warning>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr Warning& operator|=(Warning& a, Warning b) noexcept { return a = a | b; }
[[nodiscard]] constexpr bool hasWarning(Warning set, Warning flag) noexcept {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flag)) != 0U;
}

}  // namespace marspath
