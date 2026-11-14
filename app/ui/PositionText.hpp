#pragma once

#include <array>

#include "core/gps/Position.hpp"

namespace mars::ui {

	/// Room for "90.0000° S, 359.9999° E" and its terminator.
	using PositionText = std::array<char, 32>;

	/// `position` as areographic coordinates (IAU convention: planetocentric
	/// latitude, longitude positive east from 0 to 360 degrees).
	[[nodiscard]] PositionText formatPosition(t_position position) noexcept;

}
