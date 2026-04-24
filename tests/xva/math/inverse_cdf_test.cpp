/// @file inverse_cdf_test.cpp
/// @brief Unit tests validating the accuracy of the SIMD Inverse CDF.

// xva
#include "xva/math/inverse_cdf.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>

namespace xva::math::test
{

  /// @brief Helper macro for acceptable precision in our rational approximation.
  /// Acklam's algorithm guarantees absolute error <= 1.15e-9.
  constexpr double max_absolute_error = 1.15e-9;

  /// @brief Precision tolerance for standard Z-score statistical tests.
  constexpr double z_score_tolerance = 1e-6;

  /// @brief Verifies that the center of the uniform distribution (0.5) maps to exactly 0.0.
  TEST(NormalICDFTest, CentralValueMapsToZero)
  {
    using simd_f64 = NormalICDF<>::simd_f64;

    constexpr double half_prob = 0.5;
    const simd_f64 uniform(half_prob);

    const simd_f64 result = NormalICDF<>::transform(uniform);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(result[i], 0.0, max_absolute_error);
    }
  }

  /// @brief Verifies known statistical Z-scores for standard confidence intervals.
  TEST(NormalICDFTest, KnownConfidenceIntervals)
  {
    using simd_f64 = NormalICDF<>::simd_f64;

    constexpr double upper_prob = 0.977249868;
    constexpr double lower_prob = 0.022750132;
    constexpr double expected_pos = 2.0;
    constexpr double expected_neg = -2.0;

    const simd_f64 uniform_upper(upper_prob);
    const simd_f64 uniform_lower(lower_prob);

    const simd_f64 result_upper = NormalICDF<>::transform(uniform_upper);
    const simd_f64 result_lower = NormalICDF<>::transform(uniform_lower);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(result_upper[i], expected_pos, z_score_tolerance);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      EXPECT_NEAR(result_lower[i], expected_neg, z_score_tolerance);
    }
  }

  /// @brief Ensures the SIMD transformation correctly handles lanes containing disparate regions.
  TEST(NormalICDFTest, MaskingWorksAcrossDifferentRegions)
  {
    using simd_f64 = NormalICDF<>::simd_f64;

    constexpr double prob_center = 0.5;
    constexpr double prob_low = 0.01;
    constexpr double prob_high = 0.99;

    constexpr double expected_center = 0.0;
    constexpr double expected_low = -2.3263478;
    constexpr double expected_high = 2.3263478;

    const std::array<double, 3> pattern = { prob_center, prob_low, prob_high };

    alignas(simd_f64) std::array<double, simd_f64::size()> mixed_probs{};
    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      mixed_probs[i] = pattern[i % pattern.size()];
    }

    simd_f64 uniform;
    uniform.copy_from(mixed_probs.data(), std::experimental::element_aligned);

    const simd_f64 result = NormalICDF<>::transform(uniform);

    for (std::size_t i = 0; i < simd_f64::size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      const double input_prob = mixed_probs[i];
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      const double output_val = result[i];

      if (input_prob == prob_center)
      {
        EXPECT_NEAR(output_val, expected_center, max_absolute_error);
      }
      else if (input_prob == prob_low)
      {
        EXPECT_NEAR(output_val, expected_low, z_score_tolerance);
      }
      else if (input_prob == prob_high)
      {
        EXPECT_NEAR(output_val, expected_high, z_score_tolerance);
      }
    }
  }

}  // namespace xva::math::test
