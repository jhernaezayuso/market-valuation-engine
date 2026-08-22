/// @file interest_rate_swap_pricer.hpp
/// @brief High-performance SIMD pricing engine for Interest Rate Swaps.

#pragma once

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/core/simd_config.hpp"
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
  template <std::size_t SimdWidth = core::default_simd_width> class InterestRateSwapPricer
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;
    using curve_type = models::HW1FYieldCurve<SimdWidth>;

    InterestRateSwapPricer() = default;

    /// @brief Calculates the MtM of the swap across all paths and time steps.
    // NOLINTNEXTLINE(readability-function-size)
    void calculate_mtm(const core::NPVMesh& rate_mesh,
                       const std::vector<double>& time_grid,
                       const instruments::InterestRateSwap& swap,
                       const curve_type& curve,
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

      const std::vector<PeriodDiscounting> discounting = build_discounting_table(time_grid, swap, curve);

      const std::size_t num_periods = swap.periods().size();
      const std::size_t num_blocks = num_paths / simd_f64::size();
      const double notional = swap.notional();
      const bool is_payer = swap.type() == instruments::SwapType::Payer;

      tbb::parallel_for(tbb::blocked_range<std::size_t>(0, num_blocks),  // GCOVR_EXCL_LINE
                        [&](const tbb::blocked_range<std::size_t>& range) -> void
                        {
                          for (std::size_t block_idx = range.begin(); block_idx < range.end(); ++block_idx)
                          {
                            const std::size_t path_idx = block_idx * simd_f64::size();

                            for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
                            {
                              simd_f64 r_t;
                              r_t.copy_from(&rate_mesh.data()(step_idx, path_idx), std::experimental::element_aligned);

                              simd_f64 fixed_leg_pv = 0.0;
                              simd_f64 float_leg_pv = 0.0;

                              for (std::size_t period_idx = 0; period_idx < num_periods; ++period_idx)
                              {
                                const PeriodDiscounting& period = discounting[(step_idx * num_periods) + period_idx];

                                if (!period.is_active)
                                {
                                  continue;
                                }

                                const simd_f64 df_pay = curve_type::apply_coefficients(period.payment, r_t);
                                const simd_f64 df_start = curve_type::apply_coefficients(period.accrual_start, r_t);

                                fixed_leg_pv += period.fixed_coupon * df_pay;
                                float_leg_pv += notional * (df_start - df_pay);
                              }

                              const simd_f64 mtm = is_payer ? float_leg_pv - fixed_leg_pv : fixed_leg_pv - float_leg_pv;

                              mtm.copy_to(&mtm_mesh.data()(step_idx, path_idx), std::experimental::element_aligned);
                            }
                          }
                        });
    }

   private:
    /// @struct PeriodDiscounting
    /// @brief Everything one cashflow period contributes at one time step, before the scenario is known.
    struct PeriodDiscounting
    {
      models::DiscountCoefficients payment{};
      models::DiscountCoefficients accrual_start{};
      double fixed_coupon{ 0.0 };
      bool is_active{ false };
    };

    /// @brief Precomputes the part of the valuation that does not depend on the simulated rate.
    /// @param time_grid The simulation timeline.
    /// @param swap The instrument being valued.
    /// @param curve The yield curve used for discounting.
    /// @return One entry per time step and cashflow period, laid out in step major order.
    [[nodiscard]] static auto build_discounting_table(const std::vector<double>& time_grid,
                                                      const instruments::InterestRateSwap& swap,
                                                      const curve_type& curve) -> std::vector<PeriodDiscounting>
    {
      const std::vector<instruments::CashflowPeriod>& periods = swap.periods();
      std::vector<PeriodDiscounting> table(time_grid.size() * periods.size());

      for (std::size_t step_idx = 0; step_idx < time_grid.size(); ++step_idx)
      {
        const double time_t = time_grid[step_idx];

        for (std::size_t period_idx = 0; period_idx < periods.size(); ++period_idx)
        {
          const instruments::CashflowPeriod& period = periods[period_idx];
          PeriodDiscounting& entry = table[(step_idx * periods.size()) + period_idx];

          entry.is_active = period.payment_time > time_t;

          if (!entry.is_active)
          {
            continue;
          }

          const double time_start = std::max(time_t, period.start_time);

          entry.payment = curve.discount_coefficients(time_t, period.payment_time);
          entry.accrual_start = curve.discount_coefficients(time_t, time_start);
          entry.fixed_coupon = swap.notional() * swap.fixed_rate() * period.accrual_fraction;
        }
      }

      return table;
    }
  };

}  // namespace xva::pricing
