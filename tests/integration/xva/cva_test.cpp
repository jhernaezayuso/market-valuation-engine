/// @file cva_test.cpp
/// @brief Integration tests of the full pipeline: path generation, valuation, exposure and CVA.

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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xva::integration::test
{

  namespace
  {

    /// @brief Market and contract setup of the at-the-money swap under test.
    constexpr double notional = 10'000'000.0;
    constexpr double initial_rate = 0.03;
    constexpr double fixed_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double volatility = 0.01;
    constexpr double maturity = 5.0;

    /// @brief Simulation dimensions, sized to keep the suite fast while retaining exposure.
    constexpr std::size_t num_steps = 61;
    constexpr std::size_t num_paths = 4'096;
    constexpr uint32_t seed = 20'260'818;

    /// @brief Payment dates of the swap, one per year including inception.
    constexpr std::size_t num_payment_dates = 6;

    /// @brief Relative tolerance absorbing the last place gap between complementary recovery rates.
    constexpr double linearity_tolerance = 1e-12;

    /// @brief Builds the annual payment schedule of the swap.
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

    /// @brief Builds the uniform simulation timeline spanning the full horizon.
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

    /// @brief Simulates the short rate and prices the swap, populating the Mark-to-Market mesh.
    void build_mark_to_market_mesh(core::NPVMesh& mtm_mesh)
    {
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

      const pricing::InterestRateSwapPricer<> pricer;
      pricer.calculate_mtm(rate_mesh, simulation_time_grid(), swap, curve, mtm_mesh);
    }

  }  // namespace

  /// @brief Verifies that the exposure profile is never negative and vanishes at maturity.
  TEST(CvaPipelineTest, ExposureProfileIsNonNegativeAndVanishesAtMaturity)
  {
    constexpr aggregation::CounterpartyCreditProfile profile{ .recovery_rate = 0.4, .hazard_rate = 0.02 };

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaAggregator<> aggregator;
    const auto result = aggregator.calculate(mtm_mesh, simulation_time_grid(), profile);

    ASSERT_EQ(result.epe_profile.size(), num_steps);

    for (const double expected_exposure : result.epe_profile)
    {
      EXPECT_GE(expected_exposure, 0.0);
    }

    EXPECT_GT(*std::ranges::max_element(result.epe_profile), 0.0);
    EXPECT_DOUBLE_EQ(result.epe_profile.back(), 0.0);
  }

  /// @brief Verifies that the charge never exceeds the loss given default on the peak exposure.
  TEST(CvaPipelineTest, CvaIsBoundedByLossGivenDefaultOnPeakExposure)
  {
    constexpr aggregation::CounterpartyCreditProfile profile{ .recovery_rate = 0.4, .hazard_rate = 0.02 };

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaAggregator<> aggregator;
    const auto result = aggregator.calculate(mtm_mesh, simulation_time_grid(), profile);

    const double peak_exposure = *std::ranges::max_element(result.epe_profile);
    const double loss_given_default = 1.0 - profile.recovery_rate;

    EXPECT_GT(result.cva_value, 0.0);
    EXPECT_LE(result.cva_value, loss_given_default * peak_exposure);
  }

  /// @brief Verifies that a counterparty which cannot default carries no credit charge.
  TEST(CvaPipelineTest, ZeroHazardRateYieldsZeroCva)
  {
    constexpr aggregation::CounterpartyCreditProfile risk_free{ .recovery_rate = 0.4, .hazard_rate = 0.0 };

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaAggregator<> aggregator;
    const auto result = aggregator.calculate(mtm_mesh, simulation_time_grid(), risk_free);

    EXPECT_DOUBLE_EQ(result.cva_value, 0.0);
    EXPECT_GT(*std::ranges::max_element(result.epe_profile), 0.0);
  }

  /// @brief Verifies that the charge grows with the hazard rate while the exposure stays fixed.
  TEST(CvaPipelineTest, CvaIncreasesWithHazardRate)
  {
    constexpr aggregation::CounterpartyCreditProfile safe_profile{ .recovery_rate = 0.4, .hazard_rate = 0.01 };
    constexpr aggregation::CounterpartyCreditProfile risky_profile{ .recovery_rate = 0.4, .hazard_rate = 0.05 };

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaAggregator<> aggregator;
    const std::vector<double> time_grid = simulation_time_grid();

    const auto safe_result = aggregator.calculate(mtm_mesh, time_grid, safe_profile);
    const auto risky_result = aggregator.calculate(mtm_mesh, time_grid, risky_profile);

    EXPECT_GT(risky_result.cva_value, safe_result.cva_value);

    for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
    {
      EXPECT_DOUBLE_EQ(safe_result.epe_profile[step_idx], risky_result.epe_profile[step_idx]);
    }
  }

  /// @brief Verifies that the charge scales linearly with the loss given default.
  TEST(CvaPipelineTest, CvaScalesLinearlyWithLossGivenDefault)
  {
    constexpr aggregation::CounterpartyCreditProfile high_loss{ .recovery_rate = 0.4, .hazard_rate = 0.02 };
    constexpr aggregation::CounterpartyCreditProfile low_loss{ .recovery_rate = 0.7, .hazard_rate = 0.02 };

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    build_mark_to_market_mesh(mtm_mesh);

    const aggregation::CvaAggregator<> aggregator;
    const std::vector<double> time_grid = simulation_time_grid();

    const auto high_loss_result = aggregator.calculate(mtm_mesh, time_grid, high_loss);
    const auto low_loss_result = aggregator.calculate(mtm_mesh, time_grid, low_loss);

    EXPECT_NEAR(
        low_loss_result.cva_value * 2.0, high_loss_result.cva_value, high_loss_result.cva_value * linearity_tolerance);
  }

}  // namespace xva::integration::test
