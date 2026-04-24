/// @file yield_curve.hpp
/// @brief Affine Term Structure yield curve for the Hull-White 1-Factor model.

#pragma once

// xva
#include "xva/models/hull_white_1f.hpp"

// std
#include <cmath>
#include <cstddef>
#include <experimental/simd>

namespace xva::models
{

  /// @class HW1FYieldCurve
  /// @brief Computes analytical Zero-Coupon Bond prices (Discount Factors) under HW1F.
  /// @tparam SimdWidth The width of the SIMD register for double precision.
  template <std::size_t SimdWidth = 4> class HW1FYieldCurve
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;

    /// @brief Tolerance to avoid division by zero when mean reversion is negligible.
    static constexpr double reversion_tolerance = 1e-8;

    /// @brief Constructs the Yield Curve evaluator.
    /// @param params The Hull-White 1-Factor mathematical parameters.
    /// @param initial_rate The flat initial interest rate at t=0 (r_0).
    HW1FYieldCurve(const HullWhite1FParams& params, double initial_rate) : params_(params), initial_rate_(initial_rate)
    {
    }

    /// @brief Computes the discount factor from time t to T, given the simulated rate at t.
    /// @param time_t The simulation time (current observation point).
    /// @param time_T The maturity time of the cashflow (T >= t).
    /// @param simulated_r_t The vectorized simulated short rates at time t.
    /// @return The vectorized discount factors P(t, T).
    [[nodiscard]] auto forward_discount_factor(double time_t, double time_T, const simd_f64& simulated_r_t) const
        -> simd_f64
    {
      const double tau = time_T - time_t;

      if (tau <= 0.0)
      {
        return simd_f64(1.0);
      }

      double b_term = 0.0;
      double a_term = 0.0;

      constexpr double mathematical_two = 2.0;
      constexpr double mathematical_half = 0.5;
      constexpr double mathematical_four = 4.0;

      if (std::abs(params_.mean_reversion) < reversion_tolerance)
      {
        b_term = tau;
        const double vol_sq = params_.volatility * params_.volatility;
        a_term = std::exp(-mathematical_half * vol_sq * time_t* tau* tau);
      }
      else
      {
        const double mean_rev = params_.mean_reversion;
        b_term = (1.0 - std::exp(-mean_rev * tau)) / mean_rev;

        const double vol_sq = params_.volatility * params_.volatility;
        const double exp_2at = std::exp(-mathematical_two * mean_rev * time_t);
        const double p_t_T = std::exp(-initial_rate_ * tau);

        const double exp_arg =
            (b_term * initial_rate_) - (vol_sq / (mathematical_four * mean_rev)) * (1.0 - exp_2at) * (b_term * b_term);

        a_term = p_t_T * std::exp(exp_arg);
      }

      return a_term * std::experimental::exp(-b_term * simulated_r_t);
    }

    /// @brief Computes the deterministic discount factor from t=0 to T based on the initial curve.
    /// @param time_T The future time.
    /// @return The scalar discount factor P(0, T).
    [[nodiscard]] auto zero_discount_factor(double time_T) const -> double
    {
      return std::exp(-initial_rate_ * time_T);
    }

   private:
    HullWhite1FParams params_;
    double initial_rate_;
  };

}  // namespace xva::models
