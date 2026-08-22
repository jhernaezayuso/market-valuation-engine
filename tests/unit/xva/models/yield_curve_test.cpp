/// @file yield_curve_test.cpp
/// @brief Unit tests validating the Affine Term Structure calculations of the HW1F Yield Curve.

// xva
#include "xva/models/yield_curve.hpp"
#include "xva/models/hull_white_1f.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cmath>
#include <cstddef>

namespace xva::models::test
{

  /// @brief Precision tolerance for analytical bond pricing.
  constexpr double max_absolute_error = 1e-9;

  /// @brief Validates the fallback to the flat deterministic initial curve.
  TEST(HW1FYieldCurveTest, ZeroDiscountFactorMatchesFlatCurve)
  {
    constexpr double initial_rate = 0.03;
    constexpr double time_T = 5.0;

    constexpr HullWhite1FParams params{ .mean_reversion = 0.05, .long_term_mean = 0.03, .volatility = 0.01 };

    const HW1FYieldCurve<> curve(params, initial_rate);

    const double expected_discount = std::exp(-initial_rate * time_T);

    EXPECT_NEAR(curve.zero_discount_factor(time_T), expected_discount, max_absolute_error);
  }

  /// @brief Ensures that discounting from T to T evaluates to 1.0 safely.
  TEST(HW1FYieldCurveTest, ForwardDiscountAtCurrentTimeIsOne)
  {
    constexpr double initial_rate = 0.03;
    constexpr double time_t = 2.0;

    constexpr HullWhite1FParams params{ .mean_reversion = 0.05, .long_term_mean = 0.03, .volatility = 0.01 };

    const HW1FYieldCurve<> curve(params, initial_rate);

    using simd_f64 = HW1FYieldCurve<>::simd_f64;

    constexpr double simulated_rate = 0.05;
    const simd_f64 r_t(simulated_rate);

    const simd_f64 discount = curve.forward_discount_factor(time_t, time_t, r_t);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(discount[i], 1.0, max_absolute_error);
    }
  }

  /// @brief Validates the exact formulation of the Affine Term Structure against scalar math.
  TEST(HW1FYieldCurveTest, ForwardDiscountMatchesAnalyticalFormula)
  {
    constexpr double initial_rate = 0.02;
    constexpr double time_t = 1.0;
    constexpr double time_T = 3.0;

    constexpr double mean_rev = 0.05;
    constexpr double vol = 0.01;

    constexpr HullWhite1FParams params{ .mean_reversion = mean_rev, .long_term_mean = 0.02, .volatility = vol };

    const HW1FYieldCurve<> curve(params, initial_rate);

    using simd_f64 = HW1FYieldCurve<>::simd_f64;

    constexpr double simulated_rate = 0.04;
    const simd_f64 r_t(simulated_rate);

    const simd_f64 discount = curve.forward_discount_factor(time_t, time_T, r_t);

    const double tau = time_T - time_t;
    const double b_term = (1.0 - std::exp(-mean_rev * tau)) / mean_rev;

    const double p_t_T = std::exp(-initial_rate * tau);
    const double exp_2at = std::exp(-2.0 * mean_rev * time_t);
    const double a_term = p_t_T * std::exp((b_term * initial_rate) -
                                           ((vol * vol) / (4.0 * mean_rev)) * (1.0 - exp_2at) * (b_term * b_term));

    const double expected_discount = a_term * std::exp(-b_term * simulated_rate);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(discount[i], expected_discount, max_absolute_error);
    }
  }

  /// @brief Validates the forward discount factor when mean reversion approaches zero.
  TEST(HW1FYieldCurveTest, ForwardDiscountZeroMeanReversion)
  {
    constexpr double initial_rate = 0.02;
    constexpr double time_t = 1.0;
    constexpr double time_T = 3.0;

    constexpr double zero_rev = 0.0;
    constexpr double vol = 0.01;

    constexpr HullWhite1FParams params{ .mean_reversion = zero_rev, .long_term_mean = 0.02, .volatility = vol };

    const HW1FYieldCurve<> curve(params, initial_rate);

    using simd_f64 = HW1FYieldCurve<>::simd_f64;

    constexpr double simulated_rate = 0.04;
    const simd_f64 r_t(simulated_rate);

    const simd_f64 discount = curve.forward_discount_factor(time_t, time_T, r_t);

    constexpr double mathematical_half = 0.5;
    const double tau = time_T - time_t;
    const double b_term = tau;
    const double vol_sq = vol * vol;
    const double a_term = std::exp(-mathematical_half * vol_sq * time_t* tau* tau);

    const double expected_discount = a_term * std::exp(-b_term * simulated_rate);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(discount[i], expected_discount, max_absolute_error);
    }
  }

}  // namespace xva::models::test
