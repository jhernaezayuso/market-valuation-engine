/// @file cva_aggregator_test.cpp
/// @brief Unit tests validating Expected Positive Exposure extraction and CVA integration.

// xva
#include "xva/aggregation/cva_aggregator.hpp"
#include "xva/core/npv_mesh.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace xva::aggregation::test
{

  /// @brief Precision tolerance for risk metric aggregations.
  constexpr double risk_tolerance = 1e-6;

  /// @brief Ensures the aggregator correctly zeroes out negative exposures.
  TEST(CvaAggregatorTest, NegativeMtMYieldsZeroExposure)
  {
    constexpr std::size_t num_steps = 2;
    constexpr std::size_t num_paths = 8;

    const std::vector<double> time_grid = { 0.0, 1.0 };
    core::NPVMesh mtm_mesh(num_steps, num_paths);

    constexpr double dummy_negative_mtm = -500.0;

    for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
    {
      for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
      {
        mtm_mesh.data()(step_idx, path_idx) = dummy_negative_mtm;
      }
    }

    constexpr CounterpartyCreditProfile profile{ .recovery_rate = 0.4, .hazard_rate = 0.05 };

    const CvaAggregator<> aggregator;
    const auto result = aggregator.calculate(mtm_mesh, time_grid, profile);

    EXPECT_NEAR(result.epe_profile[0], 0.0, risk_tolerance);
    EXPECT_NEAR(result.epe_profile[1], 0.0, risk_tolerance);
    EXPECT_NEAR(result.cva_value, 0.0, risk_tolerance);
  }

  /// @brief Validates CVA numerical integration against an analytical expected value.
  TEST(CvaAggregatorTest, CorrectlyIntegratesConstantExposure)
  {
    constexpr std::size_t num_steps = 2;
    constexpr std::size_t num_paths = 8;

    const std::vector<double> time_grid = { 0.0, 1.0 };
    core::NPVMesh mtm_mesh(num_steps, num_paths);

    constexpr double constant_mtm = 100.0;

    for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
    {
      for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
      {
        mtm_mesh.data()(step_idx, path_idx) = constant_mtm;
      }
    }

    constexpr CounterpartyCreditProfile profile{ .recovery_rate = 0.4, .hazard_rate = 0.05 };

    const CvaAggregator<> aggregator;
    const auto result = aggregator.calculate(mtm_mesh, time_grid, profile);

    EXPECT_NEAR(result.epe_profile[0], constant_mtm, risk_tolerance);
    EXPECT_NEAR(result.epe_profile[1], constant_mtm, risk_tolerance);

    const double expected_lgd = 1.0 - profile.recovery_rate;
    const double expected_pd = std::exp(0.0) - std::exp(-profile.hazard_rate * 1.0);
    const double expected_cva = expected_lgd * constant_mtm * expected_pd;

    EXPECT_NEAR(result.cva_value, expected_cva, risk_tolerance);
  }

  /// @brief Verifies that invalid mesh sizes or path counts trigger standard exception guards.
  TEST(CvaAggregatorTest, ValidatesInputDimensions)
  {
    constexpr std::size_t num_steps = 5;
    constexpr std::size_t num_paths = 16;

    const core::NPVMesh mtm_mesh(num_steps, num_paths);
    const std::vector<double> bad_time_grid = { 0.0, 1.0 };

    constexpr std::size_t bad_num_paths = 15;
    const core::NPVMesh bad_simd_mesh(num_steps, bad_num_paths);
    const std::vector<double> good_time_grid = { 0.0, 1.0, 2.0, 3.0, 4.0 };

    constexpr CounterpartyCreditProfile profile{ .recovery_rate = 0.4, .hazard_rate = 0.05 };

    const CvaAggregator<> aggregator;

    EXPECT_THROW(static_cast<void>(aggregator.calculate(mtm_mesh, bad_time_grid, profile)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(aggregator.calculate(bad_simd_mesh, good_time_grid, profile)),
                 std::invalid_argument);
  }

}  // namespace xva::aggregation::test
