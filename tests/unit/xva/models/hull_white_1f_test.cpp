/// @file hull_white_1f_test.cpp
/// @brief Unit tests validating the SIMD Hull-White 1-Factor model.

// xva
#include "xva/models/hull_white_1f.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cmath>
#include <cstddef>

namespace xva::models::test
{

  /// @brief Acceptable floating-point error for the precomputed exact transition.
  constexpr double max_absolute_error = 1e-9;

  /// @brief Verifies the precomputed transition matches the mathematical exact solution.
  TEST(HullWhite1FTest, SingleStepMatchesExactTransition)
  {
    constexpr double mean_rev_val = 0.05;
    constexpr double long_term = 0.03;
    constexpr double vol = 0.01;
    constexpr double time_step = 1.0;
    constexpr double initial_rate = 0.02;

    constexpr HullWhite1FParams params{ .mean_reversion = mean_rev_val,
                                        .long_term_mean = long_term,
                                        .volatility = vol };

    const HullWhite1F<> model(params, time_step);

    using simd_f64 = HullWhite1F<>::simd_f64;

    constexpr double z_score = 1.0;
    const simd_f64 current_rates(initial_rate);
    const simd_f64 normal_variates(z_score);

    const simd_f64 next_rates = model.step(current_rates, normal_variates);

    constexpr double mathematical_two = 2.0;
    const double decay = std::exp(-mean_rev_val * time_step);
    const double drift = long_term * (1.0 - decay) / mean_rev_val;
    const double v_term = vol * std::sqrt((1.0 - std::exp(-mathematical_two * mean_rev_val * time_step)) /
                                          (mathematical_two * mean_rev_val));

    const double expected_rate = (initial_rate * decay) + drift + (v_term * z_score);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(next_rates[i], expected_rate, max_absolute_error);
    }
  }

  /// @brief Ensures the model gracefully degrades to Standard Brownian Motion when mean reversion is zero.
  TEST(HullWhite1FTest, ZeroMeanReversionUsesLimitFallback)
  {
    constexpr double zero_reversion = 0.0;
    constexpr double long_term = 0.03;
    constexpr double vol = 0.01;
    constexpr double time_step = 1.0;
    constexpr double initial_rate = 0.02;

    constexpr HullWhite1FParams params{ .mean_reversion = zero_reversion,
                                        .long_term_mean = long_term,
                                        .volatility = vol };

    const HullWhite1F<> model(params, time_step);

    using simd_f64 = HullWhite1F<>::simd_f64;

    constexpr double z_score = 0.0;
    const simd_f64 current_rates(initial_rate);
    const simd_f64 normal_variates(z_score);

    const simd_f64 next_rates = model.step(current_rates, normal_variates);

    const double expected_rate = initial_rate + (long_term * time_step);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(next_rates[i], expected_rate, max_absolute_error);
    }
  }

}  // namespace xva::models::test
