// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/pathfinding/IRiskMap.hpp"
#include "core/pathfinding/ITerrain.hpp"
#include "core/pathfinding/detail/LocalProjection.hpp"
#include "core/pathfinding/detail/Vec2.hpp"

namespace marspath::detail {

// Internal: every query to the user's risk map and terrain goes through this class
// (single place for projection, safety margin and query counting).

class EnvironmentProbe {
public:
    struct Config {
        double margin_m;
        double check_step_m;
        double offset_step_m;
        double max_offset_m;
    };

    EnvironmentProbe(const IRiskMap& risk, const ITerrain& terrain, const LocalProjection& projection,
                     Config config, Vec2 start, Vec2 goal) noexcept;

    [[nodiscard]] bool hazard(Vec2 p) const;
    /// Risk level of the user's map (one query, counted as a hazard query).
    [[nodiscard]] RiskLevel level(Vec2 p) const;
    /// 1 inside a caution area, 0 elsewhere (a danger is handled by hazard()).
    [[nodiscard]] double caution(Vec2 p) const { return level(p) == RiskLevel::Caution ? 1.0 : 0.0; }
    [[nodiscard]] double elevation(Vec2 p) const;

    /// Outside any hazard and at least `margin` away from one (centre + 12-point ring).
    /// The ring radius is margin + check_step/2, so every point of a segment sampled
    /// every check_step is at least `margin` away from hazards wider than the ring gap.
    [[nodiscard]] bool isClear(Vec2 p) const;
    [[nodiscard]] bool segmentClear(Vec2 a, Vec2 b) const;
    /// Middle of the first blocked run of samples along (a, b), if any.
    [[nodiscard]] std::optional<Vec2> blockedChordMiddle(Vec2 a, Vec2 b) const;

    /// First clear point along `dir` (unit vector), refined by bisection.
    [[nodiscard]] std::optional<Vec2> pushAlong(Vec2 p, Vec2 dir, double* distance = nullptr) const;
    /// Nearest exit along +-preferred (preferred wins unless 1.5x farther), then a radial search.
    [[nodiscard]] std::optional<Vec2> escape(Vec2 p, Vec2 preferred) const;

    [[nodiscard]] double checkStep() const noexcept { return config_.check_step_m; }
    [[nodiscard]] std::uint64_t hazardQueries() const noexcept { return hazard_queries_; }
    [[nodiscard]] std::uint64_t elevationQueries() const noexcept { return elevation_queries_; }

private:
    [[nodiscard]] std::optional<Vec2> nearestClear(Vec2 p) const;

    const IRiskMap& risk_;
    const ITerrain& terrain_;
    const LocalProjection& projection_;
    Config config_;
    Vec2 start_;
    Vec2 goal_;
    double ring_radius_m_;
    std::array<Vec2, 12> ring_{};
    mutable std::uint64_t hazard_queries_{0};
    mutable std::uint64_t elevation_queries_{0};
};

}  // namespace marspath::detail
