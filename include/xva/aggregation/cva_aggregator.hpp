/// @file cva_aggregator.hpp
/// @brief Parallel reduction engine to compute Expected Positive Exposure and CVA.

#pragma once

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/core/simd_config.hpp"

// tbb
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

// std
#include <cmath>
#include <cstddef>
#include <experimental/simd>
#include <functional>
#include <stdexcept>
#include <vector>

namespace xva::aggregation
{

  /// @struct CvaResult
  /// @brief Container for the final risk metrics.
  struct CvaResult
  {
    double cva_value{ 0.0 };
    std::vector<double> epe_profile;
  };

  /// @struct CounterpartyCreditProfile
  /// @brief Groups the risk parameters of the counterparty to prevent argument swapping.
  struct CounterpartyCreditProfile
  {
    double recovery_rate;
    double hazard_rate;
  };

  /// @class CvaAggregator
  /// @brief Reduces a Mark-to-Market Monte Carlo mesh into regulatory risk metrics.
  /// @tparam SimdWidth The width of the double-precision SIMD register.
  template <std::size_t SimdWidth = core::default_simd_width> class CvaAggregator
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;

    /// @brief Path blocks folded by a single leaf task, fixed so the reduction tree keeps its
    /// shape and the sum its order.
    static constexpr std::size_t reduction_grain_size = 256;

    CvaAggregator() = default;

    /// @brief Computes the EPE profile and the final CVA from an MtM mesh.
    /// @param mtm_mesh The output of a pricing engine.
    /// @param time_grid The simulation timeline.
    /// @param profile The credit risk metrics of the counterparty.
    /// @return The computed risk metrics.
    [[nodiscard]] auto calculate(const core::NPVMesh& mtm_mesh,
                                 const std::vector<double>& time_grid,
                                 const CounterpartyCreditProfile& profile) const -> CvaResult
    {
      const std::size_t num_steps = mtm_mesh.data().num_steps();
      const std::size_t num_paths = mtm_mesh.data().num_paths();

      if (time_grid.size() != num_steps) [[unlikely]]
      {
        throw std::invalid_argument("Time grid size must match the number of steps in the mesh.");
      }
      if (num_paths % simd_f64::size() != 0) [[unlikely]]
      {
        throw std::invalid_argument("Number of paths must be an exact multiple of the SIMD register width.");
      }

      CvaResult result;
      result.epe_profile.resize(num_steps, 0.0);

      const double path_factor = 1.0 / static_cast<double>(num_paths);
      const std::size_t num_blocks = num_paths / simd_f64::size();
      const simd_f64 zeros = 0.0;

      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        const double step_epe_sum = tbb::parallel_deterministic_reduce(
            tbb::blocked_range<std::size_t>(0, num_blocks, reduction_grain_size),  // GCOVR_EXCL_LINE
            0.0,                                                                   // GCOVR_EXCL_LINE
            [&](const tbb::blocked_range<std::size_t>& range, double init) -> double
            {
              double local_sum = init;
              for (std::size_t block_idx = range.begin(); block_idx < range.end(); ++block_idx)
              {
                const std::size_t path_idx = block_idx * simd_f64::size();

                simd_f64 mtm;
                mtm.copy_from(&mtm_mesh.data()(step_idx, path_idx), std::experimental::element_aligned);

                simd_f64 pos_mtm = zeros;
                std::experimental::where(mtm > zeros, pos_mtm) = mtm;

                local_sum += std::experimental::reduce(pos_mtm);
              }
              return local_sum;
            },
            std::plus<>()  // GCOVR_EXCL_LINE
        );

        result.epe_profile[step_idx] = step_epe_sum * path_factor;
      }

      const double loss_given_default = 1.0 - profile.recovery_rate;
      double cva_accumulator = 0.0;

      for (std::size_t step_idx = 1; step_idx < num_steps; ++step_idx)
      {
        const double t_prev = time_grid[step_idx - 1];
        const double t_curr = time_grid[step_idx];

        const double prob_default = std::exp(-profile.hazard_rate * t_prev) - std::exp(-profile.hazard_rate * t_curr);

        constexpr double mathematical_half = 0.5;
        const double epe_mid = mathematical_half * (result.epe_profile[step_idx - 1] + result.epe_profile[step_idx]);

        cva_accumulator += loss_given_default * epe_mid * prob_default;
      }

      result.cva_value = cva_accumulator;
      return result;
    }  // GCOVR_EXCL_LINE
  };

}  // namespace xva::aggregation
