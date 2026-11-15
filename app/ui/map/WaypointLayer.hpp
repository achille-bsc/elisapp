#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <lvgl.h>

#include "core/environment/Hazard.hpp"
#include "core/map/MapProjection.hpp"

namespace mars::ui {

	/// Tappable route point: the weather when the crew passes there.
	struct NavigationPoint {
		t_position position;
		/// Mission time at which the crew reaches it.
		std::uint32_t etaSeconds;
		std::uint32_t alongM;
		/// Risk at that time.
		HazardLevel level;
		/// 1 for the first point after the start, counted over both legs.
		std::uint8_t number;
		/// On the way back to the base.
		bool back;
	};

	/// A route vertex chosen as a navigation point.
	struct RouteMark {
		/// Index in the route, 1 or more (0 is the start).
		std::size_t vertex;
		std::uint64_t alongMm;
	};

	/// About `count` evenly spread vertices of `route`, the destination last.
	/// Returns how many were written to `out`.
	[[nodiscard]] std::size_t spreadAlongRoute(std::span<const t_position> route, std::uint32_t count, std::span<RouteMark> out);

	class INavigationPointListener {
	public:
		virtual void onNavigationPointPressed(const NavigationPoint& point) = 0;

	protected:
		~INavigationPointListener() = default;
	};

	/// Numbered route markers coloured by the risk at the time of passage.
	class WaypointLayer {
	public:
		static constexpr std::size_t kMaxPoints = 16;

		WaypointLayer(lv_obj_t* map, INavigationPointListener& listener);
		WaypointLayer(const WaypointLayer&) = delete;
		WaypointLayer& operator=(const WaypointLayer&) = delete;

		void setPoints(std::span<const NavigationPoint> points);
		void clear();
		void layout(const MapProjection& projection);

		/// See MarkerLayer::setDragFlag.
		void setDragFlag(const bool* dragging) noexcept { dragging_ = dragging; }

	private:
		static void onClicked(lv_event_t* event);

		INavigationPointListener& listener_;
		std::array<lv_obj_t*, kMaxPoints> markers_{};
		std::array<lv_obj_t*, kMaxPoints> labels_{};
		std::array<std::array<char, 4>, kMaxPoints> numbers_{};
		std::vector<NavigationPoint> points_;
		const bool* dragging_{nullptr};
	};

}
