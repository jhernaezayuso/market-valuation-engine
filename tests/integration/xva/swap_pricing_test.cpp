/// @file swap_pricing_test.cpp
/// @brief Integration tests composing the engine, the HW1F model, the yield curve and the swap pricer.

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/hull_white_1f.hpp"
#include "xva/models/yield_curve.hpp"
#include "xva/pricing/interest_rate_swap_pricer.hpp"

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

    /// @brief Market and contract setup.
    constexpr double notional = 1'000'000.0;
    constexpr double flat_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double maturity = 5.0;

    /// @brief Simulation dimensions.
    constexpr std::size_t num_steps = 6;
    constexpr std::size_t num_paths = 32;
    constexpr uint32_t seed = 42;

    /// @brief Absolute tolerance for deterministic valuations.
    constexpr double valuation_tolerance = notional * 1e-9;

    /// @struct SwapLegValues
    /// @brief Present values of both swap legs at inception.
    struct SwapLegValues
    {
      double floating_leg;
      double annuity;
    };

    /// @brief Builds the annual payment schedule covering the full horizon.
    auto annual_schedule() -> std::vector<double>
    {
      std::vector<double> schedule;
      schedule.reserve(num_steps);
      for (std::size_t period_idx = 0; period_idx < num_steps; ++period_idx)
      {
        schedule.push_back(static_cast<double>(period_idx));
      }
      return schedule;
    }

    /// @brief Builds HW1F parameters whose exact solution is a constant short rate.
    auto constant_rate_params() -> models::HullWhite1FParams
    {
      return models::HullWhite1FParams{ .mean_reversion = mean_reversion,
                                        .long_term_mean = mean_reversion * flat_rate,
                                        .volatility = 0.0 };
    }

    /// @brief Values both legs at inception on the deterministic flat curve.
    auto flat_curve_leg_values(const models::HW1FYieldCurve<>& curve, const std::vector<double>& schedule)
        -> SwapLegValues
    {
      SwapLegValues legs{ .floating_leg = curve.zero_discount_factor(schedule.front()) -
                                          curve.zero_discount_factor(schedule.back()),
                          .annuity = 0.0 };

      for (std::size_t period_idx = 1; period_idx < schedule.size(); ++period_idx)
      {
        const double accrual = schedule[period_idx] - schedule[period_idx - 1];
        legs.annuity += accrual * curve.zero_discount_factor(schedule[period_idx]);
      }

      return legs;
    }

    /// @brief Simulates the short rate and prices the swap on the resulting mesh.
    void price_swap_on_simulated_rates(const instruments::InterestRateSwap& swap,
                                       const models::HW1FYieldCurve<>& curve,
                                       core::NPVMesh& mtm_mesh)
    {
      constexpr double time_step = maturity / static_cast<double>(num_steps - 1);

      core::NPVMesh rate_mesh(num_steps, num_paths);
      const models::HullWhite1F<> model(constant_rate_params(), time_step);
      const engine::MonteCarloEngine<> mc_engine;
      mc_engine.generate_paths(rate_mesh, flat_rate, model, seed);

      std::vector<double> time_grid;
      time_grid.reserve(num_steps);
      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        time_grid.push_back(static_cast<double>(step_idx) * time_step);
      }

      const pricing::InterestRateSwapPricer<> pricer;
      pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);
    }

  }  // namespace

  /// @brief Verifies that a deterministic simulation reproduces the closed form swap value.
  TEST(SwapPricingTest, OffMarketSwapMatchesAnalyticalValue)
  {
    constexpr double off_market_fixed_rate = 0.05;

    const std::vector<double> schedule = annual_schedule();
    constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                        .notional = notional,
                                                        .fixed_rate = off_market_fixed_rate };
    const instruments::InterestRateSwap swap(terms, schedule);
    const models::HW1FYieldCurve<> curve(constant_rate_params(), flat_rate);

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    price_swap_on_simulated_rates(swap, curve, mtm_mesh);

    const SwapLegValues legs = flat_curve_leg_values(curve, schedule);
    const double expected_mtm = notional * (legs.floating_leg - (off_market_fixed_rate * legs.annuity));

    EXPECT_LT(expected_mtm, 0.0);

    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(0, path_idx), expected_mtm, valuation_tolerance);
    }
  }

  /// @brief Verifies that a swap struck at the par rate is worth nothing at inception.
  TEST(SwapPricingTest, ParSwapHasZeroValueAtInception)
  {
    const std::vector<double> schedule = annual_schedule();
    const models::HW1FYieldCurve<> curve(constant_rate_params(), flat_rate);

    const SwapLegValues legs = flat_curve_leg_values(curve, schedule);
    const double par_rate = legs.floating_leg / legs.annuity;

    const instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                    .notional = notional,
                                                    .fixed_rate = par_rate };
    const instruments::InterestRateSwap swap(terms, schedule);

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    price_swap_on_simulated_rates(swap, curve, mtm_mesh);

    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(0, path_idx), 0.0, valuation_tolerance);
    }
  }

  /// @brief Verifies that the swap value collapses to zero once the schedule is exhausted.
  TEST(SwapPricingTest, YieldsZeroValueAfterFinalPayment)
  {
    constexpr double off_market_fixed_rate = 0.05;

    const std::vector<double> schedule = annual_schedule();
    constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Receiver,
                                                        .notional = notional,
                                                        .fixed_rate = off_market_fixed_rate };
    const instruments::InterestRateSwap swap(terms, schedule);
    const models::HW1FYieldCurve<> curve(constant_rate_params(), flat_rate);

    core::NPVMesh mtm_mesh(num_steps, num_paths);
    price_swap_on_simulated_rates(swap, curve, mtm_mesh);

    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(num_steps - 1, path_idx), 0.0, valuation_tolerance);
    }
  }

}  // namespace xva::integration::test
