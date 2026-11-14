// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/detail/EnvironmentProbe.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

// Internal: coarse grid aligned with the start->goal axis, evaluated lazily.
// Memory: 5 bytes per cell (float elevation + packed state/hazard count).

class CoarseGrid {
public:
    using Index = std::uint32_t;

    void configure(Vec2 start, Vec2 goal, const GridParams& params, const EnvironmentProbe& probe);

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::size_t size() const noexcept { return z_.size(); }
    [[nodiscard]] double cellSize() const noexcept { return cell_m_; }
    [[nodiscard]] Index startCell() const noexcept { return start_cell_; }
    [[nodiscard]] Index goalCell() const noexcept { return goal_cell_; }

    [[nodiscard]] Vec2 center(Index idx) const noexcept;
    [[nodiscard]] Index indexOf(Vec2 p) const noexcept;
    /// Neighbour in direction k (see kGridSteps), if inside the grid.
    [[nodiscard]] std::optional<Index> neighbor(Index idx, std::size_t k) const noexcept;
    /// Cell sharing a corner move: column of `a`, row of `b`.
    [[nodiscard]] Index cornerCell(Index a, Index b) const noexcept {
        return (b / width_) * width_ + (a % width_);
    }

    /// Evaluates the cell on first access (1 elevation + up to 9 hazard queries).
    void ensure(Index idx);
    [[nodiscard]] bool blocked(Index idx) const noexcept { return (cells_[idx] & kStateMask) == kBlocked; }
    [[nodiscard]] double elevation(Index idx) const noexcept { return static_cast<double>(z_[idx]); }
    /// Fraction of the 9 probes that fell in a hazard (0..1).
    [[nodiscard]] double hazardFraction(Index idx) const noexcept {
        return static_cast<double>(cells_[idx] >> 2U) / 9.0;
    }
    /// Marks the cells around p as blocked (repair after a local dead end).
    void blockAround(Vec2 p);

    [[nodiscard]] std::uint32_t evaluatedCount() const noexcept { return evaluated_; }

private:
    static constexpr std::uint8_t kUnknown = 0;
    static constexpr std::uint8_t kFree = 1;
    static constexpr std::uint8_t kBlocked = 2;
    static constexpr std::uint8_t kStateMask = 3;

    std::vector<float> z_;
    std::vector<std::uint8_t> cells_;  // bits 0-1: state, bits 2-5: hazard probes hit
    const EnvironmentProbe* probe_{nullptr};
    Vec2 origin_{};
    Vec2 u_{};
    Vec2 v_{};
    double cell_m_{0.0};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    Index start_cell_{0};
    Index goal_cell_{0};
    std::uint32_t evaluated_{0};
};

}  // namespace marspath::detail
