#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/route/IRoutePlanner.hpp"

namespace mars {

	/// Out to the site, the work there, back to the base.
	struct TripSummary {
		RouteSummary outbound;
		RouteSummary back;             ///< status Ok when the way back is planned.
		std::uint32_t workSeconds;     ///< On site.
		std::uint32_t backDepartureSeconds;  ///< Mission time the way back starts.
		std::uint32_t totalSeconds;    ///< Out + work + back, on foot.
		std::uint32_t doseMicroSievert;  ///< Out + back.
		bool fitsOneEva;               ///< totalSeconds within the EVA limit.
		bool sameWayBack;              ///< The way back retraces the way out.
	};

	/// Plans both legs and keeps their waypoints.
	class TripPlanner {
	public:
		/// `sameWayMm`: within this of the base, the crew left from it.
		TripPlanner(IRoutePlanner& routes, std::uint32_t evaLimitSeconds, std::uint32_t sameWayMm = 0) noexcept
			: routes_{routes}
			, evaLimitSeconds_{evaLimitSeconds}
			, sameWayMm_{sameWayMm}
		{
		}

		/// The way back retraces the way out when the crew left from the base
		/// and it is still safe at its time; otherwise it is planned.
		const TripSummary& plan(t_position from, t_position destination, t_position base, std::uint32_t departureSeconds,
			std::uint32_t workSeconds);

		const TripSummary& planBack(t_position from, t_position base, std::uint32_t departureSeconds);

		[[nodiscard]] const TripSummary& summary() const noexcept { return summary_; }
		[[nodiscard]] std::span<const t_position> outbound() const noexcept { return outbound_; }
		[[nodiscard]] std::span<const t_position> back() const noexcept { return back_; }

	private:
		void total() noexcept;
		static void keep(const Route& route, std::vector<t_position>& into);
		bool retraceOut(t_position from, t_position base);
		[[nodiscard]] bool near(t_position a, t_position b) const noexcept;

		IRoutePlanner& routes_;
		std::uint32_t evaLimitSeconds_;
		std::uint32_t sameWayMm_;
		TripSummary summary_{};
		std::vector<t_position> outbound_;
		std::vector<t_position> back_;
	};

}
