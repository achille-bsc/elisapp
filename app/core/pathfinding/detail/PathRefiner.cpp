// SPDX-License-Identifier: MIT
#include "core/pathfinding/detail/PathRefiner.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace marspath::detail {

PathRefiner::PathRefiner(const EnvironmentProbe& probe, const LocalParams& params, Vec2 start, Vec2 goal) noexcept
    : probe_{probe}, params_{params}, start_{start},
      normal_{perp(normalized(goal - start))}, length_{norm(goal - start)} {}

void PathRefiner::resample(Path& path, double step) {
    if (path.size() < 2) {
        return;
    }
    std::size_t extra = 0;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        extra += static_cast<std::size_t>(std::max(1.0, std::ceil(norm(path[i + 1].pos() - path[i].pos()) / step))) - 1U;
    }
    Path out;
    out.reserve(path.size() + extra);
    out.push_back(path.front());
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const Vec2 a = path[i].pos();
        const Vec2 b = path[i + 1].pos();
        const int n = std::max(1, static_cast<int>(std::ceil(norm(b - a) / step)));
        for (int k = 1; k < n; ++k) {
            out.push_back(PathNode::at(a + (b - a) * (static_cast<double>(k) / n)));
        }
        out.push_back(path[i + 1]);
    }
    path.swap(out);
}

PathRefiner::Outcome PathRefiner::clearVertices(Path& path) const {
    for (std::size_t i = 1; i + 1 < path.size(); ++i) {
        if (probe_.isClear(path[i].pos())) {
            continue;
        }
        const Vec2 normal = perp(normalized(path[i + 1].pos() - path[i - 1].pos()));
        const auto q = probe_.escape(path[i].pos(), normal);
        if (!q) {
            return {false, path[i].pos()};
        }
        path[i] = PathNode::at(*q, path[i].z);
    }
    return {};
}

PathRefiner::Outcome PathRefiner::clearSegments(Path& path) const {
    const auto max_insertions = std::max<std::uint64_t>(params_.max_insertions,
                                                        static_cast<std::uint64_t>(length_ / 20.0));
    constexpr int kMaxStuck = 200;
    std::uint64_t insertions = 0;
    int stuck = 0;
    std::size_t i = 0;
    while (i + 1 < path.size()) {
        const Vec2 a = path[i].pos();
        const Vec2 b = path[i + 1].pos();
        const auto blocked = probe_.blockedChordMiddle(a, b);
        if (!blocked) {
            ++i;
            stuck = 0;
            continue;
        }
        if (++stuck > kMaxStuck || ++insertions > max_insertions) {
            return {false, *blocked};  // local maze: let the global stage route around it
        }
        Vec2 normal = perp(normalized(b - a));
        if (dot(normal, normal_) * (sideOffset(a) + sideOffset(b)) < 0.0) {
            normal = normal * -1.0;  // prefer the side the path is already on
        }
        const auto q = probe_.escape(*blocked, normal);
        if (!q) {
            return {false, *blocked};
        }
        path.insert(path.begin() + static_cast<std::ptrdiff_t>(i) + 1, PathNode::at(*q, probe_.elevation(*q)));
    }
    return {};
}

void PathRefiner::decimate(Path& path, double min_spacing) const {
    if (path.size() < 3) {
        return;
    }
    std::size_t w = 1;
    for (std::size_t i = 1; i + 1 < path.size(); ++i) {
        const Vec2 prev = path[w - 1].pos();
        if (norm(path[i].pos() - prev) < min_spacing && probe_.segmentClear(prev, path[i + 1].pos())) {
            continue;
        }
        path[w++] = path[i];
    }
    path[w++] = path.back();
    path.resize(w);
}

}  // namespace marspath::detail
