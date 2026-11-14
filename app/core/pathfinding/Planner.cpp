// SPDX-License-Identifier: MIT
#include "core/pathfinding/Planner.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/pathfinding/detail/AltitudeBand.hpp"
#include "core/pathfinding/detail/BandSearch.hpp"
#include "core/pathfinding/detail/CoarseGrid.hpp"
#include "core/pathfinding/detail/CostModel.hpp"
#include "core/pathfinding/detail/Deadline.hpp"
#include "core/pathfinding/detail/ElevationReference.hpp"
#include "core/pathfinding/detail/EnvironmentProbe.hpp"
#include "core/pathfinding/detail/GlobalPlanner.hpp"
#include "core/pathfinding/detail/GridAStar.hpp"
#include "core/pathfinding/detail/LocalProjection.hpp"
#include "core/pathfinding/detail/PathMetrics.hpp"
#include "core/pathfinding/detail/PathNode.hpp"
#include "core/pathfinding/detail/PathOptimizer.hpp"
#include "core/pathfinding/detail/PathRefiner.hpp"
#include "core/pathfinding/detail/PathSimplifier.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath {

// Orchestration only: each step is delegated to a single-responsibility component.

using detail::AltitudeBand;
using detail::CostModel;
using detail::Deadline;
using detail::ElevationReference;
using detail::EnvironmentProbe;
using detail::LocalProjection;
using detail::Path;
using detail::PathNode;
using detail::PathRefiner;
using detail::Vec2;

struct Planner::Impl {
    Impl(const IRiskMap& risk_map, const ITerrain& terrain_model, const PlannerParams& p)
        : risk{risk_map}, terrain{terrain_model}, params{p} {}

    PlanResult plan(GeoPoint start, GeoPoint goal);

    const IRiskMap& risk;
    const ITerrain& terrain;
    PlannerParams params;
    // Reusable workspace (kept between calls).
    Path path;
    detail::CoarseGrid grid;
    detail::GridAStar astar;
    detail::BandSearch band_search;
    detail::GlobalPlanner global{grid, astar, band_search};
};

namespace {

GeoPoint midpoint(GeoPoint a, GeoPoint b) noexcept {
    return {static_cast<std::int32_t>((std::int64_t{a.lat_e7} + b.lat_e7) / 2),
            static_cast<std::int32_t>((std::int64_t{a.lon_e7} + b.lon_e7) / 2)};
}

void exportPath(const Path& path, const LocalProjection& projection, GeoPoint start, GeoPoint goal,
                std::vector<Waypoint>& out) {
    out.clear();
    out.reserve(path.size());
    for (const PathNode& n : path) {
        out.push_back({projection.toGeo(n.pos()), n.z});
    }
    if (!out.empty()) {
        out.front().position = start;  // exact endpoints
        out.back().position = goal;
    }
}

}  // namespace

PlanResult Planner::Impl::plan(GeoPoint start, GeoPoint goal) {
    const Deadline clock{params.time_budget_ms};
    PlanResult result;
    if (!isValid(params)) {
        result.status = Status::InvalidParameters;
        return result;
    }
    if (risk.isHazard(start)) {
        result.status = Status::StartInHazard;
        return result;
    }
    if (risk.isHazard(goal)) {
        result.status = Status::GoalInHazard;
        return result;
    }

    const LocalProjection projection{midpoint(start, goal), params.planet_radius_m};
    const Vec2 a = projection.toLocal(start);
    const Vec2 b = projection.toLocal(goal);
    const double check_step =
        params.safety.check_step_m > 0.0 ? params.safety.check_step_m : std::max(5.0, 0.5 * params.safety.margin_m);
    const EnvironmentProbe probe{
        risk, terrain, projection,
        {params.safety.margin_m, check_step, params.local.offset_step_m, params.local.max_offset_m}, a, b};
    PlanStatistics& stats = result.stats;
    const auto finish = [&](Status status) -> PlanResult& {
        result.status = status;
        stats.hazard_queries = probe.hazardQueries();
        stats.elevation_queries = probe.elevationQueries();
        return result;
    };

    path.clear();
    if (norm(b - a) < 1.0) {
        path.push_back(PathNode::at(a, probe.elevation(a)));
        path.push_back(PathNode::at(b, probe.elevation(b)));
        detail::computeMetrics(path, stats);
        exportPath(path, projection, start, goal, result.path);
        return finish(Status::Ok);
    }

    const ElevationReference reference{a, b, probe.elevation(a), probe.elevation(b)};
    CostModel cost{params.cost, params.elevation.deviation_weight};
    AltitudeBand band;
    const PathRefiner refiner{probe, params.local, a, b};
    double sample_step = 50.0;
    double optim_step = 40.0;
    double local_ms = 0.0;

    // 1) Global coarse route, with repairs when the local stage hits a dead end.
    bool global_ok = false;
    {
        grid.configure(a, b, params.grid, probe);
        stats.grid_cell_m = grid.cellSize();
        for (std::uint8_t attempt = 0; attempt <= params.grid.max_repairs; ++attempt) {
            const auto outcome = global.plan(params.elevation, params.grid, reference, cost, band, a, b, path);
            stats.lagrange_iterations = static_cast<std::uint8_t>(stats.lagrange_iterations + outcome.iterations);
            if (!outcome.found) {
                break;
            }
            const Deadline local_clock{0.0};
            sample_step = params.local.sample_step_m > 0.0 ? params.local.sample_step_m
                                                           : std::clamp(grid.cellSize() / 2.0, 50.0, 250.0);
            optim_step = params.local.optim_step_m > 0.0 ? params.local.optim_step_m
                                                         : std::min(100.0, grid.cellSize() / 4.0);
            PathRefiner::resample(path, sample_step);
            auto refined = refiner.clearVertices(path);
            if (refined.ok) {
                refined = refiner.clearSegments(path);
            }
            local_ms += local_clock.elapsedMs();
            if (refined.ok) {
                global_ok = true;
                result.warnings |= outcome.warnings;
                break;
            }
            grid.blockAround(refined.failure);
            ++stats.repairs;
        }
        stats.grid_cells_evaluated = grid.evaluatedCount();
    }
    stats.global_ms = clock.elapsedMs() - local_ms;
    stats.used_global_search = global_ok;
    const Deadline local_clock{0.0};

    if (!global_ok) {
        return finish(Status::NoPathFound);
    }
    refiner.decimate(path, 0.3 * sample_step);

    // 3) Local optimisation, final safety pass, simplification.
    for (PathNode& n : path) {
        n.z = static_cast<float>(probe.elevation(n.pos()));
    }
    detail::PathOptimizer optimizer{probe, cost, reference, band, params.local};
    if (!optimizer.optimize(path, optim_step, clock)) {
        result.warnings |= Warning::TimeBudgetReached;
    }
    if (!refiner.clearSegments(path).ok) {
        return finish(Status::RefinementFailed);
    }
    if (params.local.simplify) {
        detail::PathSimplifier{probe, params.local}.simplify(path);
    }

    detail::computeMetrics(path, stats);
    stats.local_ms = local_ms + local_clock.elapsedMs();
    stats.final_ascent_weight = cost.ascentWeight();
    stats.final_descent_weight = cost.descentWeight();
    stats.band_low_m = band.active() ? band.low : 0.0;
    stats.band_high_m = band.active() ? band.high : 0.0;
    if (params.elevation.max_ascent_m > 0.0 && stats.ascent_m > params.elevation.max_ascent_m) {
        result.warnings |= Warning::AscentLimitExceeded;
    }
    if (params.elevation.max_descent_m > 0.0 && stats.descent_m > params.elevation.max_descent_m) {
        result.warnings |= Warning::DescentLimitExceeded;
    }
    exportPath(path, projection, start, goal, result.path);
    return finish(Status::Ok);
}

// ------------------------------------------------------------------ public API
Planner::Planner(const IRiskMap& risk_map, const ITerrain& terrain, const PlannerParams& params)
    : impl_{std::make_unique<Impl>(risk_map, terrain, params)} {}
Planner::~Planner() = default;
Planner::Planner(Planner&&) noexcept = default;
Planner& Planner::operator=(Planner&&) noexcept = default;

PlanResult Planner::plan(GeoPoint start, GeoPoint goal) { return impl_->plan(start, goal); }
void Planner::releaseMemory() { impl_ = std::make_unique<Impl>(impl_->risk, impl_->terrain, impl_->params); }

}  // namespace marspath
