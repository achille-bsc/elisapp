#include "core/route/TripPlanner.hpp"

#include <cmath>

namespace mars {

	const TripSummary& TripPlanner::plan(t_position from, t_position destination, t_position base,
		std::uint32_t departureSeconds, std::uint32_t workSeconds)
	{
		summary_ = {};
		summary_.workSeconds = workSeconds;
		const Route out = routes_.plan(from, destination, departureSeconds);
		summary_.outbound = out.summary;
		keep(out, outbound_);
		back_.clear();
		summary_.back.status = RouteStatus::Failed;
		if (out.summary.status == RouteStatus::Ok) {
			summary_.backDepartureSeconds = departureSeconds + out.summary.walkSeconds + workSeconds;
			if (!retraceOut(from, base)) {
				const Route home = routes_.plan(destination, base, summary_.backDepartureSeconds);
				summary_.back = home.summary;
				keep(home, back_);
			}
		}
		total();
		return summary_;
	}

	const TripSummary& TripPlanner::planBack(t_position from, t_position base, std::uint32_t departureSeconds)
	{
		const Route home = routes_.plan(from, base, departureSeconds);
		summary_.backDepartureSeconds = departureSeconds;
		summary_.back = home.summary;
		keep(home, back_);
		total();
		return summary_;
	}

	void TripPlanner::keep(const Route& route, std::vector<t_position>& into)
	{
		if (route.summary.status == RouteStatus::Ok) {
			into.assign(route.waypoints.begin(), route.waypoints.end());
		} else {
			into.clear();
		}
	}

	bool TripPlanner::retraceOut(t_position from, t_position base)
	{
		if (outbound_.size() < 2 || !near(from, base)) {
			return false;
		}
		std::vector<t_position> reversed(outbound_.rbegin(), outbound_.rend());
		// End at the base itself (the crew left up to sameWayMm_ from it)
		const t_position& last = reversed.back();
		if (last.latitudeE7 != base.latitudeE7 || last.longitudeEastE7 != base.longitudeEastE7) {
			reversed.push_back(base);
		}
		const RouteSummary check = routes_.assess(reversed, summary_.backDepartureSeconds);
		if (check.status != RouteStatus::Ok || check.level == HazardLevel::Danger) {
			return false;
		}
		back_ = std::move(reversed);
		summary_.back = check;
		summary_.sameWayBack = true;
		return true;
	}

	bool TripPlanner::near(t_position a, t_position b) const noexcept
	{
		// Flat distance in 1e-7 degrees: precise enough under a kilometre
		constexpr double kMmPerE7 = 3'389'500'000.0 * 3.14159265358979 / 180.0 / 1e7;
		const double dLat = static_cast<double>(a.latitudeE7 - b.latitudeE7);
		double dLon = static_cast<double>(a.longitudeEastE7) - static_cast<double>(b.longitudeEastE7);
		dLon = dLon > 1.8e9 ? dLon - 3.6e9 : (dLon < -1.8e9 ? dLon + 3.6e9 : dLon);
		const double cosLat = std::cos(static_cast<double>(a.latitudeE7) / 1e7 * 3.14159265358979 / 180.0);
		const double mm = std::hypot(dLat, dLon * cosLat) * kMmPerE7;
		return mm <= static_cast<double>(sameWayMm_);
	}

	void TripPlanner::total() noexcept
	{
		const bool out = summary_.outbound.status == RouteStatus::Ok;
		const bool home = summary_.back.status == RouteStatus::Ok;
		summary_.totalSeconds = (out ? summary_.outbound.walkSeconds : 0U) + summary_.workSeconds
			+ (home ? summary_.back.walkSeconds : 0U);
		summary_.doseMicroSievert =
			(out ? summary_.outbound.doseMicroSievert : 0U) + (home ? summary_.back.doseMicroSievert : 0U);
		summary_.fitsOneEva = out && home && summary_.totalSeconds <= evaLimitSeconds_;
	}

}
