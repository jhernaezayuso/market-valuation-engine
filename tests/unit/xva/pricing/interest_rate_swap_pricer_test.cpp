/// @file interest_rate_swap_pricer_test.cpp
/// @brief Unit tests validating the pricing logic of the Interest Rate Swap.

// xva
#include "xva/pricing/interest_rate_swap_pricer.hpp"
#include "xva/core/npv_mesh.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/hull_white_1f.hpp"
#include "xva/models/yield_curve.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace xva::pricing::test
{

  /// @brief Precision tolerance for MtM valuation matching.
  constexpr double mtm_tolerance = 1e-6;

  /// @brief Validates pricing mechanics under a flat zero-rate scenario for a Payer swap.
  TEST(InterestRateSwapPricerTest, ZeroRateScenarioYieldsExactFixedCashflowsPayer)
  {
    constexpr std::size_t num_steps = 3;
    constexpr std::size_t num_paths = 32;

    const std::vector<double> time_grid = { 0.0, 1.0, 2.0 };

    constexpr double notional = 1'000'000.0;
    constexpr double fixed_rate = 0.05;

    constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                        .notional = notional,
                                                        .fixed_rate = fixed_rate };
    const std::vector<double> schedule = { 0.0, 1.0, 2.0 };
    const instruments::InterestRateSwap swap(terms, schedule);

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = 0.05, .long_term_mean = 0.0, .volatility = 0.0 };
    const models::HW1FYieldCurve<> curve(hw_params, 0.0);

    core::NPVMesh rate_mesh(num_steps, num_paths);
    core::NPVMesh mtm_mesh(num_steps, num_paths);

    for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
    {
      for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
      {
        rate_mesh.data()(step_idx, path_idx) = 0.0;
      }
    }

    const InterestRateSwapPricer<> pricer;
    pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);

    constexpr double expected_mtm_t0 = -100'000.0;
    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(0, path_idx), expected_mtm_t0, mtm_tolerance);
    }

    constexpr double expected_mtm_t1 = -50'000.0;
    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(1, path_idx), expected_mtm_t1, mtm_tolerance);
    }

    constexpr double expected_mtm_t2 = 0.0;
    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(2, path_idx), expected_mtm_t2, mtm_tolerance);
    }
  }

  /// @brief Validates pricing mechanics under a flat zero-rate scenario for a Receiver swap.
  TEST(InterestRateSwapPricerTest, ZeroRateScenarioYieldsExactFixedCashflowsReceiver)
  {
    constexpr std::size_t num_steps = 3;
    constexpr std::size_t num_paths = 32;

    const std::vector<double> time_grid = { 0.0, 1.0, 2.0 };

    constexpr double notional = 1'000'000.0;
    constexpr double fixed_rate = 0.05;

    constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Receiver,
                                                        .notional = notional,
                                                        .fixed_rate = fixed_rate };
    const std::vector<double> schedule = { 0.0, 1.0, 2.0 };
    const instruments::InterestRateSwap swap(terms, schedule);

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = 0.05, .long_term_mean = 0.0, .volatility = 0.0 };
    const models::HW1FYieldCurve<> curve(hw_params, 0.0);

    core::NPVMesh rate_mesh(num_steps, num_paths);
    core::NPVMesh mtm_mesh(num_steps, num_paths);

    for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
    {
      for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
      {
        rate_mesh.data()(step_idx, path_idx) = 0.0;
      }
    }

    const InterestRateSwapPricer<> pricer;
    pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);

    constexpr double expected_mtm_t0 = 100'000.0;
    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      EXPECT_NEAR(mtm_mesh.data()(0, path_idx), expected_mtm_t0, mtm_tolerance);
    }
  }

  /// @brief Verifies that invalid inputs trigger strict safety validations.
  TEST(InterestRateSwapPricerTest, ValidatesInputMeshes)
  {
    constexpr std::size_t num_steps = 5;
    constexpr std::size_t num_paths = 16;

    const core::NPVMesh rate_mesh(num_steps, num_paths);

    const std::vector<double> bad_time_grid = { 0.0, 1.0 };

    core::NPVMesh bad_mtm_mesh_steps(num_steps + 1, num_paths);

    constexpr std::size_t extra_paths_valid = 8;
    core::NPVMesh bad_mtm_mesh_paths(num_steps, num_paths + extra_paths_valid);

    constexpr std::size_t extra_paths_invalid = 15;
    const core::NPVMesh bad_simd_rate_mesh(num_steps, extra_paths_invalid);
    core::NPVMesh bad_simd_mtm_mesh(num_steps, extra_paths_invalid);

    const std::vector<double> good_time_grid = { 0.0, 1.0, 2.0, 3.0, 4.0 };

    constexpr instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                        .notional = 1.0,
                                                        .fixed_rate = 0.01 };
    const instruments::InterestRateSwap swap(terms, { 0.0, 1.0 });

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = 0.05, .long_term_mean = 0.0, .volatility = 0.0 };
    const models::HW1FYieldCurve<4> curve(hw_params, 0.0);

    const InterestRateSwapPricer<4> pricer;
    core::NPVMesh mtm_mesh(num_steps, num_paths);

    EXPECT_THROW(pricer.calculate_mtm(rate_mesh, bad_time_grid, swap, curve, mtm_mesh), std::invalid_argument);
    EXPECT_THROW(pricer.calculate_mtm(rate_mesh, good_time_grid, swap, curve, bad_mtm_mesh_steps),
                 std::invalid_argument);
    EXPECT_THROW(pricer.calculate_mtm(rate_mesh, good_time_grid, swap, curve, bad_mtm_mesh_paths),
                 std::invalid_argument);
    EXPECT_THROW(pricer.calculate_mtm(bad_simd_rate_mesh, good_time_grid, swap, curve, bad_simd_mtm_mesh),
                 std::invalid_argument);
  }

}  // namespace xva::pricing::test
