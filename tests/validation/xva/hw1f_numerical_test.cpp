/// @file hw1f_numerical_test.cpp
/// @brief Numerical validation of the Monte Carlo engine against the analytical Vasicek bond price.

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
#include <vector>

namespace xva::validation::test
{

  namespace
  {

    /// @brief Tolerance of the Monte Carlo estimator at 100,000 paths.
    constexpr double convergence_tolerance = 1e-3;

    /// @brief Tolerances for the conditional moments at 200,000 paths, absolute and relative.
    constexpr double mean_tolerance = 1e-4;
    constexpr double variance_relative_tolerance = 2e-2;

    /// @brief Named divisors appearing in the closed form Vasicek expressions.
    constexpr double mathematical_two = 2.0;
    constexpr double mathematical_four = 4.0;
    constexpr double mathematical_six = 6.0;

    /// @struct VasicekParams
    struct VasicekParams
    {
      double initial_rate;
      double mean_reversion;
      double long_term_mean;
      double volatility;
    };

    /// @brief Computes the exact Zero-Coupon Bond price under Vasicek dynamics.
    auto exact_zero_coupon_bond_price(const VasicekParams& params, double maturity) -> double
    {
      constexpr double reversion_tolerance = 1e-8;
      const double variance_scale = params.volatility * params.volatility;

      if (std::abs(params.mean_reversion) < reversion_tolerance)
      {
        const double cubic_term = maturity * maturity * maturity;
        return std::exp(-params.initial_rate * maturity + (variance_scale * cubic_term / mathematical_six));
      }

      const double reversion = params.mean_reversion;
      const double b_factor = (1.0 - std::exp(-reversion * maturity)) / reversion;

      const double level_term = params.long_term_mean - (variance_scale / (mathematical_two * reversion * reversion));
      const double convexity_term = variance_scale / (mathematical_four * reversion);
      const double a_factor = std::exp((level_term * (b_factor - maturity)) - (convexity_term * b_factor * b_factor));

      return a_factor * std::exp(-b_factor * params.initial_rate);
    }

    /// @struct OrnsteinUhlenbeckMoments
    struct OrnsteinUhlenbeckMoments
    {
      double mean;
      double variance;
    };

    /// @brief Computes the exact conditional moments of the Vasicek short rate.
    auto exact_conditional_moments(const VasicekParams& params, double horizon) -> OrnsteinUhlenbeckMoments
    {
      const double decay = std::exp(-params.mean_reversion * horizon);
      const double variance_scale = params.volatility * params.volatility;

      return OrnsteinUhlenbeckMoments{ .mean = (params.initial_rate * decay) + (params.long_term_mean * (1.0 - decay)),
                                       .variance = variance_scale * (1.0 - (decay * decay)) /
                                                   (mathematical_two * params.mean_reversion) };
    }

    /// @brief Integrates the simulated short rate along every path with the trapezoid rule.
    auto integrate_short_rate_paths(const core::NPVMesh& mesh, double time_step) -> std::vector<double>
    {
      const std::size_t num_steps = mesh.data().num_steps();
      const std::size_t num_paths = mesh.data().num_paths();

      std::vector<double> path_integrals(num_paths, 0.0);

      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        const bool is_endpoint = (step_idx == 0) || (step_idx == num_steps - 1);
        const double weight = is_endpoint ? 0.5 : 1.0;

        const auto step_view = mesh.data().get_step_view(step_idx);

        for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
        {
          path_integrals[path_idx] += weight * step_view[path_idx];
        }
      }

      for (double& integral : path_integrals)
      {
        integral *= time_step;
      }

      return path_integrals;
    }

  }  // namespace

  /// @brief Validates the simulated bond price against the exact analytical solution.
  TEST(HW1FNumericalTest, MonteCarloConvergesToAnalyticalZeroCouponBond)
  {
    constexpr double initial_rate = 0.02;
    constexpr double mean_reversion = 0.05;
    constexpr double long_term_mean = 0.03;
    constexpr double volatility = 0.01;
    constexpr double maturity = 5.0;

    constexpr std::size_t num_paths = 100'000;
    constexpr std::size_t num_steps = 250;
    constexpr uint32_t seed = 42;

    constexpr double time_step = maturity / static_cast<double>(num_steps - 1);

    constexpr VasicekParams vasicek_params{ .initial_rate = initial_rate,
                                            .mean_reversion = mean_reversion,
                                            .long_term_mean = long_term_mean,
                                            .volatility = volatility };

    const double analytical_price = exact_zero_coupon_bond_price(vasicek_params, maturity);

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = mean_reversion,
                                                   .long_term_mean = mean_reversion * long_term_mean,
                                                   .volatility = volatility };

    const models::HullWhite1F<> model(hw_params, time_step);
    core::NPVMesh mesh(num_steps, num_paths);

    const engine::MonteCarloEngine<> mc_engine;
    mc_engine.generate_paths(mesh, initial_rate, model, seed);

    const auto path_integrals = integrate_short_rate_paths(mesh, time_step);

    double discount_factor_sum = 0.0;
    for (const double integral : path_integrals)
    {
      discount_factor_sum += std::exp(-integral);
    }

    const double simulated_price = discount_factor_sum / static_cast<double>(num_paths);

    EXPECT_NEAR(simulated_price, analytical_price, convergence_tolerance);
  }

  /// @brief Validates the simulated conditional moments against the exact transition law.
  TEST(HW1FNumericalTest, ConditionalMomentsMatchTheExactTransition)
  {
    constexpr double initial_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double long_term_mean = 0.05;
    constexpr double volatility = 0.01;
    constexpr double horizon = 1.0;

    constexpr std::size_t num_paths = 200'000;
    constexpr std::size_t num_steps = 21;
    constexpr uint32_t seed = 7;

    constexpr double time_step = horizon / static_cast<double>(num_steps - 1);

    constexpr VasicekParams vasicek_params{ .initial_rate = initial_rate,
                                            .mean_reversion = mean_reversion,
                                            .long_term_mean = long_term_mean,
                                            .volatility = volatility };

    const OrnsteinUhlenbeckMoments expected = exact_conditional_moments(vasicek_params, horizon);

    constexpr models::HullWhite1FParams hw_params{ .mean_reversion = mean_reversion,
                                                   .long_term_mean = mean_reversion * long_term_mean,
                                                   .volatility = volatility };

    const models::HullWhite1F<> model(hw_params, time_step);
    core::NPVMesh mesh(num_steps, num_paths);

    const engine::MonteCarloEngine<> mc_engine;
    mc_engine.generate_paths(mesh, initial_rate, model, seed);

    const auto terminal_view = mesh.data().get_step_view(num_steps - 1);

    double rate_sum = 0.0;
    for (const double rate : terminal_view)
    {
      rate_sum += rate;
    }
    const double sample_mean = rate_sum / static_cast<double>(num_paths);

    double squared_deviation_sum = 0.0;
    for (const double rate : terminal_view)
    {
      const double deviation = rate - sample_mean;
      squared_deviation_sum += deviation * deviation;
    }
    const double sample_variance = squared_deviation_sum / static_cast<double>(num_paths - 1);

    EXPECT_NEAR(sample_mean, expected.mean, mean_tolerance);
    EXPECT_NEAR(sample_variance, expected.variance, expected.variance * variance_relative_tolerance);
  }

}  // namespace xva::validation::test
