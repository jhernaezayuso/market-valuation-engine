/// @file determinism_test.cpp
/// @brief Integration tests verifying that the pipeline reproduces across thread counts and SIMD widths.

// xva
#include "xva/aggregation/cva_aggregator.hpp"
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/hull_white_1f.hpp"
#include "xva/models/yield_curve.hpp"
#include "xva/pricing/interest_rate_swap_pricer.hpp"

// tbb
#include <tbb/global_control.h>

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xva::integration::test
{

  namespace
  {

    /// @brief Model setup.
    constexpr double initial_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double volatility = 0.01;
    constexpr double time_step = 0.05;

    /// @brief Simulation dimensions, with a path count divisible by every SIMD width under test.
    constexpr std::size_t num_steps = 32;
    constexpr std::size_t num_paths = 64;
    constexpr uint32_t seed = 42;

    /// @brief Thread caps compared by the reproducibility test.
    constexpr std::size_t serial_parallelism = 1;
    constexpr std::size_t parallel_parallelism = 4;

    /// @brief Builds the model parameters used across the file.
    auto model_params() -> models::HullWhite1FParams
    {
      return models::HullWhite1FParams{ .mean_reversion = mean_reversion,
                                        .long_term_mean = mean_reversion * initial_rate,
                                        .volatility = volatility };
    }

    /// @brief Simulates the short rate and flattens the resulting mesh.
    /// @tparam SimdWidth The width of the double precision SIMD register.
    template <std::size_t SimdWidth> auto generate_flat_paths(uint32_t path_seed) -> std::vector<double>
    {
      core::NPVMesh mesh(num_steps, num_paths);
      const models::HullWhite1F<SimdWidth> model(model_params(), time_step);
      const engine::MonteCarloEngine<SimdWidth> mc_engine;
      mc_engine.generate_paths(mesh, initial_rate, model, path_seed);

      std::vector<double> flattened;
      flattened.reserve(num_steps * num_paths);

      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        const auto step_view = mesh.data().get_step_view(step_idx);
        for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
        {
          flattened.push_back(step_view[path_idx]);
        }
      }

      return flattened;
    }

    /// @brief Simulates the short rate under an explicit cap on TBB parallelism.
    /// @tparam SimdWidth The width of the double precision SIMD register.
    template <std::size_t SimdWidth> auto generate_flat_paths_capped(std::size_t max_parallelism) -> std::vector<double>
    {
      // NOLINTBEGIN(misc-include-cleaner)
      const tbb::global_control parallelism_cap(tbb::global_control::max_allowed_parallelism, max_parallelism);

      EXPECT_EQ(tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism), max_parallelism);
      // NOLINTEND(misc-include-cleaner)

      return generate_flat_paths<SimdWidth>(seed);
    }

    /// @brief Contract and dimensions of the aggregation scenario.
    constexpr double notional = 10'000'000.0;
    constexpr double fixed_rate = 0.03;
    constexpr double maturity = 5.0;
    constexpr std::size_t aggregation_num_steps = 61;
    constexpr std::size_t aggregation_num_paths = 16'384;
    constexpr std::size_t num_payment_dates = 6;

    /// @brief Credit parameters of the counterparty used by the aggregation scenario.
    constexpr aggregation::CounterpartyCreditProfile credit_profile{ .recovery_rate = 0.4, .hazard_rate = 0.02 };

    /// @brief Builds the uniform simulation timeline of the aggregation scenario.
    auto aggregation_time_grid() -> std::vector<double>
    {
      constexpr double grid_step = maturity / static_cast<double>(aggregation_num_steps - 1);

      std::vector<double> time_grid;
      time_grid.reserve(aggregation_num_steps);
      for (std::size_t step_idx = 0; step_idx < aggregation_num_steps; ++step_idx)
      {
        time_grid.push_back(static_cast<double>(step_idx) * grid_step);
      }
      return time_grid;
    }

    /// @brief Builds the annual payment schedule of the aggregation scenario.
    auto aggregation_schedule() -> std::vector<double>
    {
      std::vector<double> schedule;
      schedule.reserve(num_payment_dates);
      for (std::size_t period_idx = 0; period_idx < num_payment_dates; ++period_idx)
      {
        schedule.push_back(static_cast<double>(period_idx));
      }
      return schedule;
    }

    /// @brief Simulates and prices a swap, producing the mesh the aggregation cases reduce.
    void build_mark_to_market_mesh(core::NPVMesh& mtm_mesh)
    {
      constexpr double grid_step = maturity / static_cast<double>(aggregation_num_steps - 1);

      core::NPVMesh rate_mesh(aggregation_num_steps, aggregation_num_paths);
      const models::HullWhite1F<> model(model_params(), grid_step);
      const engine::MonteCarloEngine<> mc_engine;
      mc_engine.generate_paths(rate_mesh, initial_rate, model, seed);

      constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                          .notional = notional,
                                                          .fixed_rate = fixed_rate };
      const instruments::InterestRateSwap swap(terms, aggregation_schedule());
      const models::HW1FYieldCurve<> curve(model_params(), initial_rate);

      const pricing::InterestRateSwapPricer<> pricer;
      pricer.calculate_mtm(rate_mesh, aggregation_time_grid(), swap, curve, mtm_mesh);
    }

    /// @brief Reduces a mesh under an explicit cap on TBB parallelism.
    auto aggregate_capped(const core::NPVMesh& mtm_mesh, std::size_t max_parallelism) -> aggregation::CvaResult
    {
      // NOLINTBEGIN(misc-include-cleaner)
      const tbb::global_control parallelism_cap(tbb::global_control::max_allowed_parallelism, max_parallelism);

      EXPECT_EQ(tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism), max_parallelism);
      // NOLINTEND(misc-include-cleaner)

      const aggregation::CvaAggregator<> aggregator;
      return aggregator.calculate(mtm_mesh, aggregation_time_grid(), credit_profile);
    }

  }  // namespace

  /// @brief Verifies that the counter based generator makes results independent of the thread count.
  TEST(DeterminismTest, PathsAreBitIdenticalAcrossThreadCounts)
  {
    const std::vector<double> serial_paths = generate_flat_paths_capped<4>(serial_parallelism);
    const std::vector<double> parallel_paths = generate_flat_paths_capped<4>(parallel_parallelism);

    ASSERT_EQ(serial_paths.size(), parallel_paths.size());

    for (std::size_t value_idx = 0; value_idx < serial_paths.size(); ++value_idx)
    {
      EXPECT_EQ(serial_paths[value_idx], parallel_paths[value_idx]);
    }
  }

  /// @brief Verifies that repeated runs on the same seed reproduce the previous result exactly.
  TEST(DeterminismTest, RepeatedRunsReproduceTheSameMesh)
  {
    const std::vector<double> first_run = generate_flat_paths<4>(seed);
    const std::vector<double> second_run = generate_flat_paths<4>(seed);

    ASSERT_EQ(first_run.size(), second_run.size());

    for (std::size_t value_idx = 0; value_idx < first_run.size(); ++value_idx)
    {
      EXPECT_EQ(first_run[value_idx], second_run[value_idx]);
    }
  }

  /// @brief Verifies that a path carries the same values whatever SIMD width produced it.
  TEST(DeterminismTest, PathsAgreeAcrossSimdWidths)
  {
    const std::vector<double> width_one = generate_flat_paths<1>(seed);
    const std::vector<double> width_two = generate_flat_paths<2>(seed);
    const std::vector<double> width_four = generate_flat_paths<4>(seed);
    const std::vector<double> width_eight = generate_flat_paths<8>(seed);

    ASSERT_EQ(width_one.size(), width_four.size());
    ASSERT_EQ(width_two.size(), width_four.size());
    ASSERT_EQ(width_eight.size(), width_four.size());

    for (std::size_t value_idx = 0; value_idx < width_four.size(); ++value_idx)
    {
      EXPECT_EQ(width_one[value_idx], width_four[value_idx]);
      EXPECT_EQ(width_two[value_idx], width_four[value_idx]);
      EXPECT_EQ(width_eight[value_idx], width_four[value_idx]);
    }
  }

  /// @brief Verifies that distinct seeds drive the simulation down different trajectories.
  TEST(DeterminismTest, DistinctSeedsProduceDistinctPaths)
  {
    constexpr uint32_t alternative_seed = 4'242;

    const std::vector<double> base_paths = generate_flat_paths<4>(seed);
    const std::vector<double> alternative_paths = generate_flat_paths<4>(alternative_seed);

    ASSERT_EQ(base_paths.size(), alternative_paths.size());

    std::size_t differing_values = 0;
    for (std::size_t value_idx = 0; value_idx < base_paths.size(); ++value_idx)
    {
      if (base_paths[value_idx] != alternative_paths[value_idx])
      {
        ++differing_values;
      }
    }

    EXPECT_GT(differing_values, num_paths);
  }

  /// @brief Verifies that the exposure profile and the credit charge do not depend on the thread count.
  TEST(DeterminismTest, ExposureAndChargeAreBitIdenticalAcrossThreadCounts)
  {
    core::NPVMesh mtm_mesh(aggregation_num_steps, aggregation_num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaResult serial_result = aggregate_capped(mtm_mesh, serial_parallelism);
    const aggregation::CvaResult parallel_result = aggregate_capped(mtm_mesh, parallel_parallelism);

    ASSERT_EQ(serial_result.epe_profile.size(), parallel_result.epe_profile.size());

    for (std::size_t step_idx = 0; step_idx < serial_result.epe_profile.size(); ++step_idx)
    {
      EXPECT_EQ(serial_result.epe_profile[step_idx], parallel_result.epe_profile[step_idx]);
    }

    EXPECT_EQ(serial_result.cva_value, parallel_result.cva_value);
  }

  /// @brief Verifies that reducing the same mesh twice returns exactly the same figures.
  TEST(DeterminismTest, RepeatedAggregationReproducesTheSameCharge)
  {
    core::NPVMesh mtm_mesh(aggregation_num_steps, aggregation_num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaAggregator<> aggregator;
    const std::vector<double> time_grid = aggregation_time_grid();

    const aggregation::CvaResult first_result = aggregator.calculate(mtm_mesh, time_grid, credit_profile);
    const aggregation::CvaResult second_result = aggregator.calculate(mtm_mesh, time_grid, credit_profile);

    ASSERT_EQ(first_result.epe_profile.size(), second_result.epe_profile.size());

    for (std::size_t step_idx = 0; step_idx < first_result.epe_profile.size(); ++step_idx)
    {
      EXPECT_EQ(first_result.epe_profile[step_idx], second_result.epe_profile[step_idx]);
    }

    EXPECT_EQ(first_result.cva_value, second_result.cva_value);
  }

}  // namespace xva::integration::test
