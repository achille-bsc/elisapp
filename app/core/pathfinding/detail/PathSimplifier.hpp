// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/detail/EnvironmentProbe.hpp"
#include "core/pathfinding/detail/PathNode.hpp"

namespace marspath::detail {

// Internal: removes vertices that are collinear in 3D (plan and elevation).

class PathSimplifier {
public:
    PathSimplifier(const EnvironmentProbe& probe, const LocalParams& params) noexcept
        : probe_{probe}, params_{params} {}

    /// Single pass: from an anchor, extend the segment while every skipped vertex stays
    /// within tolerance of the 3D line; one hazard check per kept segment. In place.
    void simplify(Path& path) const;

private:
    [[nodiscard]] bool withinTolerance(const Path& path, std::size_t i, std::size_t j) const noexcept;

    const EnvironmentProbe& probe_;
    const LocalParams& params_;
};

}  // namespace marspath::detail
