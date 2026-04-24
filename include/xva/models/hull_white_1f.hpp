/// @file hull_white_1f.hpp
/// @brief Hull-White 1-Factor (HW1F) model for interest rate simulation.

#pragma once

// std
#include <cmath>
#include <cstddef>
#include <experimental/simd>

namespace xva::models
{

  /// @struct HullWhite1FParams
  /// @brief Configuration parameters for the Hull-White 1-Factor model.
  struct HullWhite1FParams
  {
    double mean_reversion;
    double long_term_mean;
    double volatility;
  };

  /// @class HullWhite1F
  /// @brief SIMD implementation of the HW1F short rate model.
  /// @tparam SimdWidth The width of the SIMD register for double precision.
  template <std::size_t SimdWidth = 4> class HullWhite1F
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;

    /// @brief Tolerance to avoid division by zero when mean reversion is negligible.
    static constexpr double reversion_tolerance = 1e-8;

    /// @brief Constructs the HW1F model and precomputes the exact transition constants.
    /// @param params The struct containing the mathematical parameters of the model.
    /// @param time_step The discrete time step increment (Delta t).
    HullWhite1F(const HullWhite1FParams& params, double time_step)
        : decay_factor_(std::exp(-params.mean_reversion * time_step))
    {
      constexpr double mathematical_two = 2.0;

      if (std::abs(params.mean_reversion) < reversion_tolerance)
      {
        drift_term_ = params.long_term_mean * time_step;
        vol_term_ = params.volatility * std::sqrt(time_step);
      }
      else
      {
        drift_term_ = params.long_term_mean * (1.0 - decay_factor_) / params.mean_reversion;
        vol_term_ =
            params.volatility * std::sqrt((1.0 - std::exp(-mathematical_two * params.mean_reversion * time_step)) /
                                          (mathematical_two * params.mean_reversion));
      }
    }

    /// @brief Advances the interest rate by one time step using SIMD vectors.
    /// @param current_rate The vectorized short rates at time t.
    /// @param normal_variate The vectorized standard normal random variables (Z).
    /// @return The vectorized short rates at time t + dt.
    [[nodiscard]] auto step(const simd_f64& current_rate, const simd_f64& normal_variate) const -> simd_f64
    {
      return (current_rate * decay_factor_) + drift_term_ + (vol_term_ * normal_variate);
    }

   private:
    double decay_factor_{ 0.0 };
    double drift_term_{ 0.0 };
    double vol_term_{ 0.0 };
  };

}  // namespace xva::models
