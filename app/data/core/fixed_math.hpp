#pragma once

#include "./coord.hpp"
#include <cstdint>

// Fixed point maths. Tables are built at compile time (consteval: double is
// allowed there and never reaches the binary). At run time only integers are
// left: no libm, no FPU, same result on every target.

// Constants //
constexpr uint32_t Q15_SHIFT = 15;
constexpr uint32_t Q16_SHIFT = 16;
constexpr uint32_t Q30_SHIFT = 30;
constexpr int32_t Q15_ONE = 1 << Q15_SHIFT;
constexpr uint32_t Q16_ONE = 1u << Q16_SHIFT;
constexpr int64_t Q30_ONE = int64_t{1} << Q30_SHIFT;
constexpr double PI = 3.14159265358979323846;
constexpr double DEG_PER_HALF_TURN = 180.0;
// Taylor terms: plenty for |x| <= 2 pi in double precision
constexpr int32_t TAYLOR_TERMS = 40;

// cos Taylor series up to x^14: error below 1e-9 on [0, 90] deg
constexpr int64_t COS_SERIES_TERMS = 7;
// Samples of one period of a periodic cosine, both ends included
constexpr uint32_t PERIODIC_STEPS = 48;
// periodic_cos_q15 keeps (phase remainder * table step) in 32 bits
constexpr uint32_t PERIOD_MAX = 200000;

// Compile time helpers //
consteval double ce_exp(double x) {
  double term = 1.0;
  double sum = 1.0;
  for (int32_t n = 1; n < TAYLOR_TERMS; n++) {
    term *= x / static_cast<double>(n);
    sum += term;
  }
  return (sum);
}

consteval double ce_cos(double x) {
  double term = 1.0;
  double sum = 1.0;
  for (int32_t n = 1; n < TAYLOR_TERMS; n++) {
    term *= -x * x / static_cast<double>((2 * n - 1) * (2 * n));
    sum += term;
  }
  return (sum);
}

consteval int64_t ce_round(double v) {
  return (v >= 0.0 ? static_cast<int64_t>(v + 0.5)
                   : -static_cast<int64_t>(-v + 0.5));
}

// Tables //
struct s_periodic_table {
  int32_t q15[PERIODIC_STEPS + 1];
} typedef t_periodic_table;

consteval t_periodic_table make_periodic_table() {
  t_periodic_table table = {};
  for (uint32_t k = 0; k <= PERIODIC_STEPS; k++)
    table.q15[k] = static_cast<int32_t>(
        ce_round(ce_cos(2.0 * PI * static_cast<double>(k) /
                        static_cast<double>(PERIODIC_STEPS)) *
                 Q15_ONE));
  return (table);
}

// Micro-degrees -> radians in Q30, the factor itself in Q24 (3e-9 relative)
constexpr uint32_t RAD_FACTOR_SHIFT = 24;
inline constexpr int64_t RAD_Q30_PER_UDEG_Q24 = ce_round(
    PI / DEG_PER_HALF_TURN / UDEG_PER_DEG * Q30_ONE * (int64_t{1} << RAD_FACTOR_SHIFT));
// cos_q30 accuracy, in Q30 units (1.5e-8)
constexpr int64_t COS_Q30_TOLERANCE = 16;
inline constexpr t_periodic_table PERIODIC_TABLE = make_periodic_table();

// Functions //
// Division rounded half away from zero, den > 0
constexpr int64_t div_round(int64_t num, int64_t den) {
  return (num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den));
}

// cos of an angle in [0, 90] deg given in micro-degrees, Q30 (2^30 = 1).
// Horner form of the Taylor series: 1 - x2/(1*2) * (1 - x2/(3*4) * (...))
constexpr int64_t cos_q30(uint32_t abs_udeg) {
  if (abs_udeg >= static_cast<uint32_t>(LAT_MAX_UDEG))
    return (0);
  const int64_t x = (int64_t{abs_udeg} * RAD_Q30_PER_UDEG_Q24) >> RAD_FACTOR_SHIFT;
  const int64_t x2 = (x * x) >> Q30_SHIFT;
  int64_t t = Q30_ONE;
  for (int64_t n = COS_SERIES_TERMS; n > 0; n--)
    t = Q30_ONE - ((x2 * t) >> Q30_SHIFT) / ((2 * n - 1) * (2 * n));
  return (t);
}

// cos(2 pi * phase / period), Q15 signed, period <= PERIOD_MAX
constexpr int32_t periodic_cos_q15(uint32_t phase, uint32_t period) {
  const uint32_t scaled = (phase % period) * PERIODIC_STEPS;
  const uint32_t step = scaled / period;
  const int32_t rest = static_cast<int32_t>(scaled % period);
  const int32_t from = PERIODIC_TABLE.q15[step];
  const int32_t to = PERIODIC_TABLE.q15[step + 1];
  return (from + (to - from) * rest / static_cast<int32_t>(period));
}

// Highest power of 4 in a uint64_t: start of the bit by bit square root
constexpr uint64_t ISQRT_TOP_BIT = uint64_t{1} << 62;

// floor(sqrt(n)), bit by bit
constexpr uint32_t isqrt_u64(uint64_t n) {
  uint64_t root = 0;
  uint64_t bit = ISQRT_TOP_BIT;
  while (bit > n)
    bit >>= 2;
  while (bit != 0) {
    if (n >= root + bit) {
      n -= root + bit;
      root = (root >> 1) + bit;
    } else
      root >>= 1;
    bit >>= 2;
  }
  return (static_cast<uint32_t>(root));
}

// cos(0) = 1, cos(60 deg) = 0.5 to a few Q30 units, cos(90 deg) = 0
static_assert(cos_q30(0) == Q30_ONE);
static_assert(cos_q30(60000000) - Q30_ONE / 2 <= COS_Q30_TOLERANCE &&
              Q30_ONE / 2 - cos_q30(60000000) <= COS_Q30_TOLERANCE);
static_assert(cos_q30(static_cast<uint32_t>(LAT_MAX_UDEG)) == 0);
// x2 * t: (pi/2)^2 * 2^30 * 2^30 stays far below 2^63
static_assert((RAD_Q30_PER_UDEG_Q24 * LAT_MAX_UDEG >> RAD_FACTOR_SHIFT) < 2 * Q30_ONE);
static_assert(RAD_Q30_PER_UDEG_Q24 < INT64_MAX / LAT_MAX_UDEG);
static_assert(PERIODIC_TABLE.q15[0] == Q15_ONE);
static_assert(PERIODIC_TABLE.q15[PERIODIC_STEPS / 2] == -Q15_ONE);
static_assert(PERIODIC_TABLE.q15[PERIODIC_STEPS] == Q15_ONE);
static_assert(PERIODIC_TABLE.q15[PERIODIC_STEPS / 4] == 0);
static_assert(periodic_cos_q15(0, 1000) == Q15_ONE);
static_assert(periodic_cos_q15(500, 1000) == -Q15_ONE);
static_assert(periodic_cos_q15(1000, 1000) == Q15_ONE);
static_assert(uint64_t{PERIOD_MAX} * PERIODIC_STEPS < UINT32_MAX);
// Interpolation product: (max step drop ~ Q15 * 2 pi / steps) * period
static_assert(int64_t{Q15_ONE} * PERIOD_MAX / PERIODIC_STEPS * 7 < INT32_MAX);
static_assert(div_round(7, 2) == 4 && div_round(-7, 2) == -4);
static_assert(div_round(5, 3) == 2 && div_round(-5, 3) == -2);
static_assert(isqrt_u64(0) == 0 && isqrt_u64(15) == 3 && isqrt_u64(16) == 4);
static_assert(isqrt_u64(uint64_t{UINT32_MAX} * UINT32_MAX) == UINT32_MAX);
