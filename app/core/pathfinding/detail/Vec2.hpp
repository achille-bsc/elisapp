// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>

namespace marspath::detail {

struct Vec2 {
    double x{0.0};
    double y{0.0};
};
[[nodiscard]] constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
[[nodiscard]] constexpr Vec2 operator*(Vec2 a, double k) noexcept { return {a.x * k, a.y * k}; }
[[nodiscard]] constexpr double dot(Vec2 a, Vec2 b) noexcept { return a.x * b.x + a.y * b.y; }
[[nodiscard]] constexpr Vec2 perp(Vec2 a) noexcept { return {-a.y, a.x}; }
[[nodiscard]] inline double norm(Vec2 a) noexcept { return std::sqrt(dot(a, a)); }
[[nodiscard]] inline Vec2 normalized(Vec2 a) noexcept {
    const double n = norm(a);
    return n > 1e-12 ? a * (1.0 / n) : Vec2{};
}

}  // namespace marspath::detail
