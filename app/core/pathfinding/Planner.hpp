// SPDX-License-Identifier: MIT
#pragma once

#include <memory>

#include "core/pathfinding/GeoPoint.hpp"
#include "core/pathfinding/IRiskMap.hpp"
#include "core/pathfinding/ITerrain.hpp"
#include "core/pathfinding/PlanResult.hpp"
#include "core/pathfinding/PlannerParams.hpp"

namespace marspath {

/// Plans a route between two points that avoids hazards (with a safety margin) and
/// limits elevation changes.
///
/// The planner keeps its working buffers between calls (no reallocation when planning
/// repeatedly). The risk map and terrain must outlive the planner.
/// Not thread-safe: use one Planner per thread.
class Planner {
public:
    Planner(const IRiskMap& risk_map, const ITerrain& terrain, const PlannerParams& params = {});
    ~Planner();
    Planner(Planner&&) noexcept;
    Planner& operator=(Planner&&) noexcept;
    Planner(const Planner&) = delete;
    Planner& operator=(const Planner&) = delete;

    [[nodiscard]] PlanResult plan(GeoPoint start, GeoPoint goal);

    /// Frees the working buffers (they are re-created on the next plan()).
    void releaseMemory();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace marspath
