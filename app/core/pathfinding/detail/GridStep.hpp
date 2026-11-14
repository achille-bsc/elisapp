// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>

namespace marspath::detail {

/// 8-connected neighbourhood. Index k is stored as the parent direction in A*.
struct GridStep {
    std::int8_t di;
    std::int8_t dj;
};
inline constexpr std::array<GridStep, 8> kGridSteps{
    {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};

}  // namespace marspath::detail
