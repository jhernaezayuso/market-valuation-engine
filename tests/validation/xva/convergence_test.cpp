/// @file convergence_test.cpp
/// @brief Numerical validation of the Monte Carlo convergence order of the estimator.

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/models/hull_white_1f.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace xva::validation::test
{

  namespace
  {

    /// @brief Model setup.
    constexpr double initial_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double long_term_mean = 0.05;
    constexpr double volatility = 0.01;
    constexpr double time_step = 0.25;

    /// @brief Simulation dimensions.
    constexpr std::size_t num_steps = 2;
    constexpr std::size_t num_replications = 32;

    /// @brief Path counts in a ratio of four.
    constexpr std::size_t small_sample = 4'096;
    constexpr std::size_t medium_sample = 16'384;
    constexpr std::size_t large_sample = 65'536;

    /// @brief Bounds on the measured error ratio.
    constexpr double minimum_error_ratio = 1.3;
    constexpr double maximum_error_ratio = 3.0;

    /// @brief Relative band around the analytical standard error.
    constexpr double standard_error_relative_tolerance = 0.4;

    /// @brief Named factor appearing in the closed form Ornstein-Uhlenbeck variance.
    constexpr double mathematical_two = 2.0;

    /// @struct ReplicationSetup
    struct ReplicationSetup
    {
      std::size_t path_count;
      uint32_t run_seed;
    };

    /// @brief Builds the model parameters.
    auto model_params() -> models::HullWhite1FParams
    {
      return models::HullWhite1FParams{ .mean_reversion = mean_reversion,
                                        .long_term_mean = mean_reversion * long_term_mean,
                                        .volatility = volatility };
    }

    /// @brief Computes the exact conditional mean of the short rate after one transition.
    auto exact_terminal_mean() -> double
    {
      const double decay = std::exp(-mean_reversion * time_step);
      return (initial_rate * decay) + (long_term_mean * (1.0 - decay));
    }

    /// @brief Computes the exact standard deviation of the short rate after one transition.
    auto exact_terminal_deviation() -> double
    {
      const double variance =
          (1.0 - std::exp(-mathematical_two * mean_reversion * time_step)) / (mathematical_two * mean_reversion);
      return volatility * std::sqrt(variance);
    }

    /// @brief Averages the simulated short rate at the terminal mesh point.
    auto simulated_terminal_mean(const ReplicationSetup& setup) -> double
    {
      core::NPVMesh mesh(num_steps, setup.path_count);
      const models::HullWhite1F<> model(model_params(), time_step);
      const engine::MonteCarloEngine<> mc_engine;
      mc_engine.generate_paths(mesh, initial_rate, model, setup.run_seed);

      double rate_sum = 0.0;
      for (const double rate : mesh.data().get_step_view(num_steps - 1))
      {
        rate_sum += rate;
      }

      return rate_sum / static_cast<double>(setup.path_count);
    }

    /// @brief Measures the root mean square error of the estimator over independent replications.
    auto root_mean_square_error(std::size_t path_count) -> double
    {
      const double exact_mean = exact_terminal_mean();
      double squared_error_sum = 0.0;

      for (std::size_t replication_idx = 0; replication_idx < num_replications; ++replication_idx)
      {
        const ReplicationSetup setup{ .path_count = path_count,
                                      .run_seed = static_cast<uint32_t>(replication_idx + 1) };
        const double error = simulated_terminal_mean(setup) - exact_mean;
        squared_error_sum += error * error;
      }

      return std::sqrt(squared_error_sum / static_cast<double>(num_replications));
    }

    /// @brief Asserts the measured error agrees with the analytical standard error at one sample size.
    void expect_error_matches_analytical_standard_error(std::size_t path_count)
    {
      const double theoretical_error = exact_terminal_deviation() / std::sqrt(static_cast<double>(path_count));
      const double measured_error = root_mean_square_error(path_count);

      EXPECT_NEAR(measured_error, theoretical_error, theoretical_error * standard_error_relative_tolerance);
    }

  }  // namespace

  /// @brief Verifies that the sampling error falls as the inverse square root of the path count.
  TEST(ConvergenceTest, SamplingErrorHalvesWhenPathCountQuadruples)
  {
    const double small_error = root_mean_square_error(small_sample);
    const double medium_error = root_mean_square_error(medium_sample);
    const double large_error = root_mean_square_error(large_sample);

    EXPECT_LT(medium_error, small_error);
    EXPECT_LT(large_error, medium_error);

    const double first_ratio = small_error / medium_error;
    const double second_ratio = medium_error / large_error;

    EXPECT_GT(first_ratio, minimum_error_ratio);
    EXPECT_LT(first_ratio, maximum_error_ratio);
    EXPECT_GT(second_ratio, minimum_error_ratio);
    EXPECT_LT(second_ratio, maximum_error_ratio);
  }

  /// @brief Verifies that the size of the sampling error matches the analytical standard error.
  TEST(ConvergenceTest, SamplingErrorMatchesTheAnalyticalStandardError)
  {
    expect_error_matches_analytical_standard_error(small_sample);
    expect_error_matches_analytical_standard_error(medium_sample);
    expect_error_matches_analytical_standard_error(large_sample);
  }

}  // namespace xva::validation::test
