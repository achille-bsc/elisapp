// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/CoarseGrid.hpp"

#include <algorithm>
#include <cmath>

#include "core/pathfinding/detail/GridStep.hpp"

namespace marspath::detail {

void CoarseGrid::configure(Vec2 start, Vec2 goal, const GridParams& params, const EnvironmentProbe& probe) {
    probe_ = &probe;
    const double length = norm(goal - start);
    const double extension = std::max(params.corridor_min_m, 0.1 * length);
    const double half_width = std::max(params.corridor_min_m, params.corridor_ratio * length);
    u_ = normalized(goal - start);
    v_ = perp(u_);
    cell_m_ = std::max(params.min_cell_m, (length + 2.0 * extension) / static_cast<double>(params.max_side));
    width_ = static_cast<std::uint32_t>(std::ceil((length + 2.0 * extension) / cell_m_)) + 1U;
    height_ = static_cast<std::uint32_t>(std::ceil(2.0 * half_width / cell_m_)) + 1U;
    origin_ = start - u_ * extension - v_ * (static_cast<double>((height_ - 1U) / 2U) * cell_m_);

    const std::size_t n = std::size_t{width_} * height_;
    z_.assign(n, 0.0F);
    cells_.assign(n, kUnknown);
    evaluated_ = 0;

    start_cell_ = indexOf(start);
    goal_cell_ = indexOf(goal);
    for (const Index c : {start_cell_, goal_cell_}) {
        ensure(c);
        cells_[c] = kFree;  // forced free, no hazard penalty
    }
}

Vec2 CoarseGrid::center(Index idx) const noexcept {
    return origin_ + u_ * (static_cast<double>(idx % width_) * cell_m_) +
           v_ * (static_cast<double>(idx / width_) * cell_m_);
}

CoarseGrid::Index CoarseGrid::indexOf(Vec2 p) const noexcept {
    const auto clampIndex = [](double v, std::uint32_t n) {
        return static_cast<std::uint32_t>(std::clamp(std::lround(v), 0L, static_cast<long>(n) - 1L));
    };
    const std::uint32_t i = clampIndex(dot(p - origin_, u_) / cell_m_, width_);
    const std::uint32_t j = clampIndex(dot(p - origin_, v_) / cell_m_, height_);
    return j * width_ + i;
}

std::optional<CoarseGrid::Index> CoarseGrid::neighbor(Index idx, std::size_t k) const noexcept {
    const auto i = static_cast<std::int64_t>(idx % width_) + kGridSteps[k].di;
    const auto j = static_cast<std::int64_t>(idx / width_) + kGridSteps[k].dj;
    if (i < 0 || j < 0 || i >= static_cast<std::int64_t>(width_) || j >= static_cast<std::int64_t>(height_)) {
        return std::nullopt;
    }
    return static_cast<Index>(j * width_ + i);
}

void CoarseGrid::ensure(Index idx) {
    if ((cells_[idx] & kStateMask) != kUnknown) {
        return;
    }
    const Vec2 c = center(idx);
    z_[idx] = static_cast<float>(probe_->elevation(c));
    // 3x3 probes spread over the cell. Blocked if the centre or at least 6/9 probes are
    // hazardous; otherwise passable with a penalty (the local stage handles the rest).
    const double s = cell_m_ / 3.0;
    std::uint8_t hits = 0;
    bool centre_hit = false;
    for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
            if (probe_->hazard(c + u_ * (di * s) + v_ * (dj * s))) {
                ++hits;
                centre_hit = centre_hit || (di == 0 && dj == 0);
            }
        }
    }
    const std::uint8_t state = (centre_hit || hits >= 6) ? kBlocked : kFree;
    cells_[idx] = static_cast<std::uint8_t>(state | (hits << 2U));
    ++evaluated_;
}

void CoarseGrid::blockAround(Vec2 p) {
    const Index c = indexOf(p);
    for (std::size_t k = 0; k <= kGridSteps.size(); ++k) {
        const std::optional<Index> n = (k == kGridSteps.size()) ? std::optional<Index>{c} : neighbor(c, k);
        if (!n || *n == start_cell_ || *n == goal_cell_) {
            continue;
        }
        ensure(*n);
        cells_[*n] = static_cast<std::uint8_t>((cells_[*n] & ~kStateMask) | kBlocked);
    }
}

}  // namespace marspath::detail
