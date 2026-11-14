// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

namespace marspath {

enum class Status : std::uint8_t {
    Ok,
    InvalidParameters,
    StartInHazard,
    GoalInHazard,
    NoPathFound,
    RefinementFailed,
};

[[nodiscard]] std::string_view toString(Status status) noexcept;

}  // namespace marspath
