// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>

namespace marspath::detail {

/// Monotonic stopwatch with an optional deadline.
class Deadline {
public:
    explicit Deadline(double budget_ms) noexcept : start_{Clock::now()}, budget_ms_{budget_ms} {}

    [[nodiscard]] double elapsedMs() const noexcept {
        return std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    }
    [[nodiscard]] bool expired() const noexcept { return budget_ms_ > 0.0 && elapsedMs() > budget_ms_; }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
    double budget_ms_;
};

}  // namespace marspath::detail
