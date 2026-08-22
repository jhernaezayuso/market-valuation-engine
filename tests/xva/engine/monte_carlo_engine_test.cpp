/// @file monte_carlo_engine_test.cpp
/// @brief Unit tests validating the orchestrator and parallel Monte Carlo convergence.

// xva
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/core/npv_mesh.hpp"
#include "xva/models/hull_white_1f.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace xva::engine::test
{

  /// @brief Verifies that the engine respects architectural dimension constraints.
  TEST(MonteCarloEngineTest, ValidatesPathSimdMultiples)
  {
    constexpr std::size_t invalid_num_paths = 15;
    constexpr std::size_t num_steps = 10;
    constexpr double spot_rate = 0.02;
    constexpr double time_step = 1.0;
    constexpr uint32_t seed = 42;

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = 0.05, .long_term_mean = 0.03, .volatility = 0.01 };

    core::NPVMesh mesh(num_steps, invalid_num_paths);
    const models::HullWhite1F<> model(hw_params, time_step);
    const MonteCarloEngine<> engine;

    EXPECT_THROW(engine.generate_paths(mesh, spot_rate, model, seed), std::invalid_argument);
  }

  /// @brief Ensures the engine rejects simulation requests with insufficient time steps.
  TEST(MonteCarloEngineTest, ValidatesMinimumTimeSteps)
  {
    constexpr std::size_t valid_num_paths = 16;
    constexpr std::size_t invalid_num_steps = 1;
    constexpr double spot_rate = 0.02;
    constexpr double time_step = 1.0;
    constexpr uint32_t seed = 42;

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = 0.05, .long_term_mean = 0.03, .volatility = 0.01 };

    core::NPVMesh mesh(invalid_num_steps, valid_num_paths);
    const models::HullWhite1F<> model(hw_params, time_step);
    const MonteCarloEngine<> engine;

    EXPECT_THROW(engine.generate_paths(mesh, spot_rate, model, seed), std::invalid_argument);
  }

  /// @brief Verifies that the uniform mapping never reaches the endpoints of the unit interval.
  TEST(MonteCarloEngineTest, UniformMappingStaysInsideOpenUnitInterval)
  {
    constexpr double lowest_word = 0.0;
    constexpr auto highest_word = static_cast<double>(std::numeric_limits<uint32_t>::max());

    constexpr double lowest_uniform =
        (lowest_word + MonteCarloEngine<>::u32_to_unit_offset) / MonteCarloEngine<>::u32_to_double_divisor;
    constexpr double highest_uniform =
        (highest_word + MonteCarloEngine<>::u32_to_unit_offset) / MonteCarloEngine<>::u32_to_double_divisor;

    EXPECT_GT(lowest_uniform, 0.0);
    EXPECT_LT(highest_uniform, 1.0);
  }

  /// @brief Validates the Martingale property of the Monte Carlo simulation using HW1F.
  /// @details With zero mean reversion and zero long term mean, the expected value
  /// of the short rate in the future E[r_T] must equal the initial rate r_0.
  TEST(MonteCarloEngineTest, FollowsMartingalePropertyUnderZeroDrift)
  {
    constexpr std::size_t num_paths = 100'000;
    constexpr std::size_t num_steps = 10;

    constexpr double spot_rate = 0.02;
    constexpr double time_step = 0.1;
    constexpr uint32_t seed = 12345;

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = 0.0, .long_term_mean = 0.0, .volatility = 0.01 };

    constexpr double convergence_tolerance = 0.005;

    core::NPVMesh mesh(num_steps, num_paths);
    const models::HullWhite1F<> model(hw_params, time_step);
    const MonteCarloEngine<> engine;

    engine.generate_paths(mesh, spot_rate, model, seed);

    double sum_final_rates = 0.0;
    const std::size_t final_step_idx = num_steps - 1;

    const auto final_rates_view = std::as_const(mesh).data().get_step_view(final_step_idx);

    for (const double rate : final_rates_view)
    {
      sum_final_rates += rate;
    }

    const double average_final_rate = sum_final_rates / static_cast<double>(num_paths);

    EXPECT_NEAR(average_final_rate, spot_rate, convergence_tolerance);
  }

}  // namespace xva::engine::test
