/// @file reference_values_test.cpp
/// @brief Checks a bit level fingerprint of the pipeline against its reference value.

// xva
#include "xva/aggregation/cva_aggregator.hpp"
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/hull_white_1f.hpp"
#include "xva/models/yield_curve.hpp"
#include "xva/pricing/interest_rate_swap_pricer.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xva::validation::test
{

  namespace
  {

    /// @brief Market and contract setup of the reference scenario.
    constexpr double notional = 10'000'000.0;
    constexpr double initial_rate = 0.03;
    constexpr double fixed_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double volatility = 0.01;
    constexpr double maturity = 5.0;

    /// @brief Simulation dimensions of the reference scenario.
    constexpr std::size_t num_steps = 61;
    constexpr std::size_t num_paths = 4'096;
    constexpr std::size_t num_payment_dates = 6;
    constexpr uint32_t seed = 42;

    /// @brief Credit parameters of the reference counterparty.
    constexpr double recovery_rate = 0.4;
    constexpr double hazard_rate = 0.02;

    /// @brief FNV-1a parameters, used to fold the mesh into a single value.
    constexpr uint64_t fnv_offset_basis = 14695981039346656037ULL;
    constexpr uint64_t fnv_prime = 1099511628211ULL;
    constexpr uint64_t byte_mask = 0xFF;
    constexpr std::size_t bits_per_byte = 8;
    constexpr std::size_t bytes_per_double = 8;

    /// @brief Fingerprints of the reference scenario, held only without floating point contraction.
    constexpr uint64_t reference_rate_mesh = 0x04F5'FCA0'A0D6'A14BULL;
    constexpr uint64_t reference_mtm_mesh = 0xC713'5EF8'B3FB'B650ULL;

    /// @brief Builds the annual payment schedule of the reference swap.
    auto annual_schedule() -> std::vector<double>
    {
      std::vector<double> schedule;
      schedule.reserve(num_payment_dates);
      for (std::size_t period_idx = 0; period_idx < num_payment_dates; ++period_idx)
      {
        schedule.push_back(static_cast<double>(period_idx));
      }
      return schedule;
    }

    /// @brief Builds the uniform simulation timeline.
    auto simulation_time_grid() -> std::vector<double>
    {
      constexpr double time_step = maturity / static_cast<double>(num_steps - 1);

      std::vector<double> time_grid;
      time_grid.reserve(num_steps);
      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        time_grid.push_back(static_cast<double>(step_idx) * time_step);
      }
      return time_grid;
    }

    /// @brief Folds every cell of a mesh into one value, bit for bit.
    auto fingerprint(const core::NPVMesh& mesh) -> uint64_t
    {
      uint64_t hash = fnv_offset_basis;

      for (std::size_t step_idx = 0; step_idx < mesh.data().num_steps(); ++step_idx)
      {
        const auto step_view = mesh.data().get_step_view(step_idx);

        for (const double value : step_view)
        {
          const auto bits = std::bit_cast<uint64_t>(value);

          for (std::size_t byte_idx = 0; byte_idx < bytes_per_double; ++byte_idx)
          {
            hash ^= (bits >> (byte_idx * bits_per_byte)) & byte_mask;
            hash *= fnv_prime;
          }
        }
      }

      return hash;
    }

  }  // namespace

  /// @brief Verifies that every cell of both meshes still holds the same bits.
  TEST(ReferenceValuesTest, PipelineReproducesTheReferenceFingerprint)
  {
#ifndef XVA_FP_CONTRACT_OFF
    GTEST_SKIP();
#endif

    constexpr double time_step = maturity / static_cast<double>(num_steps - 1);

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = mean_reversion,
                                                   .long_term_mean = mean_reversion * initial_rate,
                                                   .volatility = volatility };

    core::NPVMesh rate_mesh(num_steps, num_paths);
    const models::HullWhite1F<> model(hw_params, time_step);
    const engine::MonteCarloEngine<> mc_engine;
    mc_engine.generate_paths(rate_mesh, initial_rate, model, seed);

    constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                        .notional = notional,
                                                        .fixed_rate = fixed_rate };
    const instruments::InterestRateSwap swap(terms, annual_schedule());
    const models::HW1FYieldCurve<> curve(hw_params, initial_rate);
    const std::vector<double> time_grid = simulation_time_grid();

    const pricing::InterestRateSwapPricer<> pricer;
    core::NPVMesh mtm_mesh(num_steps, num_paths);
    pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);

    constexpr aggregation::CounterpartyCreditProfile profile{ .recovery_rate = recovery_rate,
                                                              .hazard_rate = hazard_rate };
    const aggregation::CvaAggregator<> aggregator;
    const auto result = aggregator.calculate(mtm_mesh, time_grid, profile);

    EXPECT_EQ(fingerprint(rate_mesh), reference_rate_mesh);
    EXPECT_EQ(fingerprint(mtm_mesh), reference_mtm_mesh);
    EXPECT_GT(result.cva_value, 0.0);
  }

}  // namespace xva::validation::test
