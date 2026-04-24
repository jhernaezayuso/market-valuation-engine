/// @file interest_rate_swap_pricer.hpp
/// @brief High-performance SIMD pricing engine for Interest Rate Swaps.

#pragma once

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/yield_curve.hpp"

// tbb
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

// std
#include <algorithm>
#include <cstddef>
#include <experimental/simd>
#include <stdexcept>
#include <vector>

namespace xva::pricing
{

  /// @class InterestRateSwapPricer
  /// @brief Evaluates the Mark-to-Market value of an IRS across a Monte Carlo simulation.
  /// @tparam SimdWidth The width of the double-precision SIMD register.
  template <std::size_t SimdWidth = 4> class InterestRateSwapPricer
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;

    InterestRateSwapPricer() = default;

    /// @brief Calculates the MtM of the swap across all paths and time steps.
    // NOLINTNEXTLINE(readability-function-size)
    void calculate_mtm(const core::NPVMesh& rate_mesh,
                       const std::vector<double>& time_grid,
                       const instruments::InterestRateSwap& swap,
                       const models::HW1FYieldCurve<SimdWidth>& curve,
                       core::NPVMesh& mtm_mesh) const
    {
      const std::size_t num_steps = rate_mesh.data().num_steps();
      const std::size_t num_paths = rate_mesh.data().num_paths();

      if (time_grid.size() != num_steps) [[unlikely]]
      {
        throw std::invalid_argument("Time grid size must match the number of steps in the rate mesh.");
      }

      if (mtm_mesh.data().num_steps() != num_steps || mtm_mesh.data().num_paths() != num_paths) [[unlikely]]
      {
        throw std::invalid_argument("Output MtM mesh dimensions must match the input rate mesh.");
      }

      if (num_paths % simd_f64::size() != 0) [[unlikely]]
      {
        throw std::invalid_argument("Number of paths must be an exact multiple of the SIMD register width.");
      }

      const std::size_t num_blocks = num_paths / simd_f64::size();

      tbb::parallel_for(
          tbb::blocked_range<std::size_t>(0, num_blocks),  // GCOVR_EXCL_LINE
          [&](const tbb::blocked_range<std::size_t>& range) -> void
          {
            for (std::size_t block_idx = range.begin(); block_idx < range.end(); ++block_idx)
            {
              const std::size_t path_idx = block_idx * simd_f64::size();

              for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
              {
                const double time_t = time_grid[step_idx];

                simd_f64 r_t;
                r_t.copy_from(&rate_mesh.data()(step_idx, path_idx), std::experimental::element_aligned);

                simd_f64 fixed_leg_pv = 0.0;
                simd_f64 float_leg_pv = 0.0;

                for (const auto& period : swap.periods())
                {
                  if (period.payment_time > time_t)
                  {
                    const simd_f64 df_pay = curve.forward_discount_factor(time_t, period.payment_time, r_t);

                    fixed_leg_pv += swap.notional() * swap.fixed_rate() * period.accrual_fraction * df_pay;

                    const double time_start = std::max(time_t, period.start_time);
                    const simd_f64 df_start = curve.forward_discount_factor(time_t, time_start, r_t);

                    float_leg_pv += swap.notional() * (df_start - df_pay);
                  }
                }

                simd_f64 mtm = 0.0;
                if (swap.type() == instruments::SwapType::Payer)
                {
                  mtm = float_leg_pv - fixed_leg_pv;
                }
                else
                {
                  mtm = fixed_leg_pv - float_leg_pv;
                }

                mtm.copy_to(&mtm_mesh.data()(step_idx, path_idx), std::experimental::element_aligned);
              }
            }
          });
    }
  };

}  // namespace xva::pricing
