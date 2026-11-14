// SPDX-License-Identifier: MIT
#include "core/pathfinding/Status.hpp"

namespace marspath {

std::string_view toString(Status status) noexcept {
    switch (status) {
        case Status::Ok: return "ok";
        case Status::InvalidParameters: return "invalid parameters";
        case Status::StartInHazard: return "start lies in a hazard";
        case Status::GoalInHazard: return "goal lies in a hazard";
        case Status::NoPathFound: return "no path found";
        case Status::RefinementFailed: return "local refinement failed";
    }
    return "unknown";
}

}  // namespace marspath
