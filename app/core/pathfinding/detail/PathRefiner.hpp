// SPDX-License-Identifier: MIT
#pragma once

#include "core/pathfinding/PlannerParams.hpp"
#include "core/pathfinding/detail/EnvironmentProbe.hpp"
#include "core/pathfinding/detail/PathNode.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

// Internal: geometric operations that make a path hazard-free with the safety margin.

class PathRefiner {
public:
    struct Outcome {
        bool ok{true};
        Vec2 failure{};  ///< Where the refinement got stuck (for the repair loop).
    };

    PathRefiner(const EnvironmentProbe& probe, const LocalParams& params, Vec2 start, Vec2 goal) noexcept;

    /// Adds vertices so that no segment is longer than `step` (new vertices: z = 0).
    static void resample(Path& path, double step);
    /// Moves every hazardous vertex to the nearest clear point.
    [[nodiscard]] Outcome clearVertices(Path& path) const;
    /// Inserts vertices until every segment is clear (samples every check step).
    [[nodiscard]] Outcome clearSegments(Path& path) const;
    /// Removes vertices closer than `min_spacing` to their predecessor when safe.
    void decimate(Path& path, double min_spacing) const;

private:
    [[nodiscard]] double sideOffset(Vec2 p) const noexcept { return dot(p - start_, normal_); }

    const EnvironmentProbe& probe_;
    const LocalParams& params_;
    Vec2 start_;
    Vec2 normal_;
    double length_;
};

}  // namespace marspath::detail
