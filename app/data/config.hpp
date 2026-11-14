#pragma once

#include <cstdint>

// Every tunable of the data module lives here. Units are in the names.
// Sources: SOURCES.txt at the repo root.

// Mars //
// Mean solar day: 24 h 39 min 35 s
constexpr uint32_t MARS_SOL_S = 88775;
// One Mars "hour" = 1/24 sol (local true solar time)
constexpr uint32_t MARS_HOURS_PER_SOL = 24;
constexpr uint32_t MARS_HOUR_S = MARS_SOL_S / MARS_HOURS_PER_SOL;
// Atmospheric scale height (NASA Mars fact sheet: 11.1 km)
constexpr int32_t MARS_SCALE_HEIGHT_M = 11100;
// Mean radius, same value as the pathfinding (PlannerParams::planet_radius_m)
constexpr uint32_t MARS_RADIUS_MM = 3389500000u;

// Weather satellite //
// Built-in cache: 16 chunks of 8x8 deg (7.5 KB) cover a 500 km zone at low and
// mid latitudes. Bigger trips: caller sized cache, see weather_set_cache().
constexpr uint8_t WEATHER_CACHE_SLOTS = 16;

// Local weather evolution (diurnal cycle, weather_model.cpp) //
// Thermal tide: column mass varies about +-5 % around the median (Guo 2015)
constexpr uint16_t PRESSURE_DIURNAL_AMPLITUDE_PERMILLE = 50;
// shortcut: peak hours are approximations (REMS/MEDA papers not reached),
// replace them by fitted values for the landing site
constexpr uint32_t PRESSURE_PEAK_HOUR = 8;
constexpr uint16_t TEMPERATURE_DIURNAL_AMPLITUDE_DK = 360; // +-36 K
constexpr uint32_t TEMPERATURE_PEAK_HOUR = 14;

// Radiation (radiation.cpp), linear fit of Guo et al. 2015, RAD detector E //
// D = D01 + KAPPA * (P - P0) + BETA * PHI
constexpr int32_t RAD_DOSE_INTERCEPT_NGY_DAY = 267170;   // D01'
constexpr int32_t RAD_PRESSURE_SLOPE_NGY_DAY_DPA = -13;  // -0.13 uGy/day/Pa
constexpr int32_t RAD_PRESSURE_REF_DPA = 8400;           // P0 = 840 Pa
constexpr int32_t RAD_SOLAR_SLOPE_NGY_DAY_MV = -110;     // -0.11 uGy/day/MV
// Solar modulation potential PHI: higher = stronger sun = fewer cosmic rays.
// Update it from space weather; ~450 MV matches the 0.21 mGy/day of 2012-13.
constexpr int32_t SOLAR_MODULATION_MV = 450;
// Dose equivalent / absorbed dose at Gale: 0.64 mSv / 0.21 mGy (Hassler 2014)
constexpr uint32_t RAD_QUALITY_FACTOR_PERCENT = 305;
// Without satellite data: pressure at the datum, MOLA areoid definition
// (water triple point, 610.5 Pa), then corrected by the elevation
constexpr uint16_t DATUM_PRESSURE_DEFAULT_DPA = 6105;

// Radiation limits, NASA-STD-3001 Vol. 1 Rev B (OCHMO-TB-020) //
// [V1 4030] career effective dose from space flight < 600 mSv, all ages/sexes
constexpr uint32_t RAD_CAREER_LIMIT_USV = 600000;
// [V1 4031] design reference solar particle event < 250 mSv per event
constexpr uint32_t RAD_SPE_EVENT_LIMIT_USV = 250000;
// Dose the crew already has before this surface stay (transit, earlier
// flights): set it per crew from the dosimetry
constexpr uint32_t RAD_CREW_DOSE_RECEIVED_USV = 0;
// shortcut: planned surface stay, a typical long stay; set it per mission
constexpr uint32_t RAD_SURFACE_STAY_SOLS = 500;
// Critical dose rate: kept for the whole stay, it spends the rest of the
// career limit. GCR alone (~0.65 mSv/sol) stays below it.
constexpr uint16_t RADIATION_CRITICAL_USV_DAY = static_cast<uint16_t>(
    (RAD_CAREER_LIMIT_USV - RAD_CREW_DOSE_RECEIVED_USV) / RAD_SURFACE_STAY_SOLS);
// [V1 4029] ALARA: caution from this share of the critical rate (team choice)
constexpr uint32_t RAD_CAUTION_PERCENT = 80;

// Hazard zones (hazard.cpp) //
// xEVA concept of operations: suited walking up, down and across slopes up to
// 20 deg
constexpr uint8_t HAZARD_SLOPE_DANGER_HALF_DEG = 40;
// shortcut: team choice, 5 deg of margin under the suit capability
constexpr uint8_t HAZARD_SLOPE_CAUTION_HALF_DEG = 30;
// A slope read on a coarser layer (MOLA, 1.85 km) hides steep ground: caution
constexpr uint32_t HAZARD_FINE_TERRAIN_MAX_STEP_UDEG = 1000; // ~60 m
// shortcut: no NASA norm found, placeholders to calibrate
constexpr uint16_t HAZARD_WIND_CMS = 2500; // 25 m/s
constexpr uint8_t HAZARD_DUST_X40 = 80;    // tau 2: dust storm

// Traverse on foot (hazard_route) //
// xEMU / xEVA: one EVA lasts up to 8 hours
constexpr uint32_t EVA_MAX_DURATION_S = 8 * 3600;
// NASA Mars Exploration Zone (2016): surface traverse out to a 100 km radius
constexpr uint32_t EXPLORATION_ZONE_RADIUS_MM = 100000000;
// shortcut: assumed suited walking speed, calibrate it
constexpr uint32_t WALK_SPEED_MM_S = 1000;

static_assert(RAD_CREW_DOSE_RECEIVED_USV < RAD_CAREER_LIMIT_USV);
static_assert((RAD_CAREER_LIMIT_USV - RAD_CREW_DOSE_RECEIVED_USV) /
                  RAD_SURFACE_STAY_SOLS <=
              UINT16_MAX);
static_assert(RAD_CAUTION_PERCENT < 100);
static_assert(HAZARD_SLOPE_CAUTION_HALF_DEG < HAZARD_SLOPE_DANGER_HALF_DEG);
