/// @file inverse_cdf.hpp
/// @brief Vectorized Inverse Cumulative Distribution Function (ICDF) for Normal Distributions.

#pragma once

// xva
#include "xva/core/simd_config.hpp"

// std
#include <cstddef>
#include <experimental/simd>

namespace xva::math
{

  /// @class NormalICDF
  /// @brief SIMD implementation of the Inverse Normal CDF.
  /// Transforms uniformly distributed random numbers in the range (0, 1)
  /// into standard normal distributed numbers (mean = 0, variance = 1)
  /// @tparam SimdWidth The width of the SIMD register.
  template <std::size_t SimdWidth = core::default_simd_width> class NormalICDF
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;
    using mask_type = typename simd_f64::mask_type;

    // NOLINTBEGIN(readability-identifier-length)
    /// @brief Acklam's algorithm constants for the central region.
    static constexpr double a1 = -3.969683028665376e+01;
    static constexpr double a2 = 2.209460984245205e+02;
    static constexpr double a3 = -2.759285104469687e+02;
    static constexpr double a4 = 1.383577518672690e+02;
    static constexpr double a5 = -3.066479806614716e+01;
    static constexpr double a6 = 2.506628277459239e+00;

    static constexpr double b1 = -5.447609879822406e+01;
    static constexpr double b2 = 1.615858368580409e+02;
    static constexpr double b3 = -1.556989798598866e+02;
    static constexpr double b4 = 6.680131188771972e+01;
    static constexpr double b5 = -1.328068155288572e+01;

    /// @brief Acklam's algorithm constants for the tail regions.
    static constexpr double c1 = -7.784894002430293e-03;
    static constexpr double c2 = -3.223964580411365e-01;
    static constexpr double c3 = -2.400758277161838e+00;
    static constexpr double c4 = -2.549732539343734e+00;
    static constexpr double c5 = 4.374664141464968e+00;
    static constexpr double c6 = 2.938163982698783e+00;

    static constexpr double d1 = 7.784695709041462e-03;
    static constexpr double d2 = 3.224671290700398e-01;
    static constexpr double d3 = 2.445134137142996e+00;
    static constexpr double d4 = 3.754408661907416e+00;
    // NOLINTEND(readability-identifier-length)

    /// @brief Thresholds determining whether a uniform value lies in the center or the tails.
    static constexpr double low_threshold = 0.02425;
    static constexpr double high_threshold = 1.0 - low_threshold;
    static constexpr double half = 0.5;

    /// @brief Transforms a vectorized uniform distribution to a normal distribution.
    /// @param uniform A SIMD vector containing probabilities in the range (0, 1).
    /// @return A SIMD vector containing values following a standard normal distribution.
    /// @details Lane selection stays branchless. The guards test the whole register, and skipping
    /// a tail is safe because its mask would assign nothing.
    [[nodiscard]] static auto transform(const simd_f64& uniform) -> simd_f64
    {
      const mask_type is_low = uniform < low_threshold;
      const mask_type is_high = uniform > high_threshold;

      const simd_f64 q_center = uniform - half;
      const simd_f64 r_center = q_center * q_center;

      const simd_f64 num_center =
          (((((a1 * r_center + a2) * r_center + a3) * r_center + a4) * r_center + a5) * r_center + a6) * q_center;
      const simd_f64 den_center =
          ((((b1 * r_center + b2) * r_center + b3) * r_center + b4) * r_center + b5) * r_center + 1.0;

      simd_f64 result = num_center / den_center;

      if (std::experimental::any_of(is_low))
      {
        const simd_f64 sqrt_low = std::experimental::sqrt(-2.0 * std::experimental::log(uniform));
        const simd_f64 num_low =
            ((((c1 * sqrt_low + c2) * sqrt_low + c3) * sqrt_low + c4) * sqrt_low + c5) * sqrt_low + c6;
        const simd_f64 den_low = (((d1 * sqrt_low + d2) * sqrt_low + d3) * sqrt_low + d4) * sqrt_low + 1.0;

        std::experimental::where(is_low, result) = num_low / den_low;
      }

      if (std::experimental::any_of(is_high))
      {
        const simd_f64 q_high = 1.0 - uniform;
        const simd_f64 sqrt_high = std::experimental::sqrt(-2.0 * std::experimental::log(q_high));
        const simd_f64 num_high =
            ((((c1 * sqrt_high + c2) * sqrt_high + c3) * sqrt_high + c4) * sqrt_high + c5) * sqrt_high + c6;
        const simd_f64 den_high = (((d1 * sqrt_high + d2) * sqrt_high + d3) * sqrt_high + d4) * sqrt_high + 1.0;

        std::experimental::where(is_high, result) = -(num_high / den_high);
      }

      return result;
    }
  };

}  // namespace xva::math
