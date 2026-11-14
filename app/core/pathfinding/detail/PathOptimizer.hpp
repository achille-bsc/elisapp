// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/detail/AltitudeBand.hpp"
#include "core/pathfinding/detail/CostModel.hpp"
#include "core/pathfinding/detail/Deadline.hpp"
#include "core/pathfinding/detail/ElevationReference.hpp"
#include "core/pathfinding/detail/EnvironmentProbe.hpp"
#include "core/pathfinding/detail/PathNode.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

// Internal: local optimisation of a hazard-free path (never leaves the clear area).

/// Moves "bumps" of vertices (whole sections, triangular weights) sideways, from large
/// windows down to single vertices, with a decreasing step, while the cost decreases.
class PathOptimizer {
public:
    PathOptimizer(const EnvironmentProbe& probe, const CostModel& cost, const ElevationReference& reference,
                  const AltitudeBand& band, const LocalParams& params) noexcept
        : probe_{probe}, cost_{cost}, reference_{reference}, band_{band}, params_{params} {}

    /// Returns false if the deadline stopped the optimisation early.
    [[nodiscard]] bool optimize(Path& path, double initial_step, const Deadline& deadline);

    /// Cost of segment (u, v), used by the optimiser.
    [[nodiscard]] double segmentCost(const PathNode& u, const PathNode& v) const noexcept;

private:
    [[nodiscard]] double rangeCost(const Path& path, std::size_t lo, std::size_t hi) const noexcept;
    void applyBump(Path& path, std::size_t lo, std::size_t hi, std::size_t center, int window, Vec2 move) const;
    [[nodiscard]] bool rangeClear(const Path& path, std::size_t lo, std::size_t hi) const;

    const EnvironmentProbe& probe_;
    const CostModel& cost_;
    const ElevationReference& reference_;
    const AltitudeBand& band_;
    const LocalParams& params_;
    Path backup_;
};

}  // namespace marspath::detail
