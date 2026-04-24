/// @file main.cpp
/// @brief CLI app for Interest Rate Swap CVA calculation using the xVA engine.

// xva
#include "xva/aggregation/cva_aggregator.hpp"
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/hull_white_1f.hpp"
#include "xva/models/yield_curve.hpp"
#include "xva/pricing/interest_rate_swap_pricer.hpp"

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

  /// @brief Configuration extracted from CLI arguments.
  struct AppConfig
  {
    std::size_t num_paths{ 100'000 };
    uint32_t seed{ 42 };
    double notional{ 10'000'000.0 };
    bool verbose{ true };
    bool use_bps{ false };
  };

  /// @brief Constant to convert decimals to basis points.
  constexpr double bps_multiplier = 10000.0;

  /// @brief Parses command line arguments into an AppConfig structure.
  /// @param args Span of character pointers from main.
  /// @param config Reference to the configuration to be populated.
  /// @return True if parsing succeeded and execution should continue.
  auto parse_arguments(const std::span<const char* const> args, AppConfig& config) -> bool
  {
    for (std::size_t i = 1; i < args.size(); ++i)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      const std::string_view arg{ args[i] };

      if (arg == "--paths" && i + 1 < args.size())
      {
        config.num_paths = std::stoull(std::string(args[++i]));
      }
      else if (arg == "--seed" && i + 1 < args.size())
      {
        config.seed = static_cast<uint32_t>(std::stoul(std::string(args[++i])));
      }
      else if (arg == "--notional" && i + 1 < args.size())
      {
        config.notional = std::stod(std::string(args[++i]));
      }
      else if (arg == "--bps")
      {
        config.use_bps = true;
      }
      else if (arg == "--quiet" || arg == "-q")
      {
        config.verbose = false;
      }
      else if (arg == "--help" || arg == "-h")
      {
        std::println("Usage: swap_cva [options]");
        std::println("Options:");
        std::println("  --paths    <N> Number of Monte Carlo paths (default: 100,000)");
        std::println("  --notional <N> Contract principal amount (default: 10,000,000)");
        std::println("  --seed     <N> Random generator seed (default: 42)");
        std::println("  --bps          Format CVA output in Basis Points");
        std::println("  -q, --quiet    Minimal output");
        std::println("  -h, --help     Show this help message");
        return false;
      }
      else
      {
        std::println(stderr, "[ERROR] Unknown argument: {}", arg);
        return false;
      }
    }
    return true;
  }

  /// @brief Prints a table with the compute time of each engine component.
  /// @param ms_sim Path generation time in milliseconds.
  /// @param ms_prc Pricing time in milliseconds.
  /// @param ms_agg Aggregation time in milliseconds.
  void print_performance_report(int64_t ms_sim, int64_t ms_prc, int64_t ms_agg)
  {
    std::println("\n-------------------------");
    std::println(" PERFORMANCE METRICS");
    std::println("-------------------------");
    std::println(" Path Generation : {} ms", ms_sim);
    std::println(" Swap Pricing    : {} ms", ms_prc);
    std::println(" CVA Aggregation : {} ms", ms_agg);
    std::println(" Total Compute   : {} ms", ms_sim + ms_prc + ms_agg);
    std::println("-------------------------\n");
  }

  /// @brief Handles the aggregation and console reporting of the risk metrics.
  /// @param config Application configuration.
  /// @param mtm_mesh Calculated Mark-to-Market grid.
  /// @param time_grid Simulation timeline.
  void print_risk_report(const AppConfig& config,
                         const xva::core::NPVMesh& mtm_mesh,
                         const std::vector<double>& time_grid)
  {
    const std::string_view unit = config.use_bps ? "(bps)" : "(CCY)";

    std::println("================================================================");
    std::println(" CVA SWAP - Notional: {:.2f}", config.notional);
    std::println("----------------------------------------------------------------");
    std::println("{:<15} | {:<12} | {:<12} | CVA Charge {}", "Risk Profile", "Hazard Rate", "Recovery", unit);
    std::println("----------------------------------------------------------------");

    const xva::aggregation::CvaAggregator<> aggregator;

    struct Scenario
    {
      std::string_view name;
      xva::aggregation::CounterpartyCreditProfile profile;
    };

    const std::vector<Scenario> scenarios = { { "Low Risk", { .recovery_rate = 0.40, .hazard_rate = 0.01 } },
                                              { "Medium Risk", { .recovery_rate = 0.35, .hazard_rate = 0.03 } },
                                              { "High Risk", { .recovery_rate = 0.30, .hazard_rate = 0.06 } } };

    for (const auto& [name, profile] : scenarios)
    {
      const auto res = aggregator.calculate(mtm_mesh, time_grid, profile);
      const double val = config.use_bps ? (res.cva_value / config.notional) * bps_multiplier : res.cva_value;
      std::println("{:<15} | {:<12.2f} | {:<12.2f} | {:<15.2f}", name, profile.hazard_rate, profile.recovery_rate, val);
    }
    std::println("================================================================\n");
  }

  /// @brief Executes the core Monte Carlo simulation workflow.
  /// @param config Application configuration.
  void run_simulation(AppConfig& config)
  {
    constexpr std::size_t num_years = 5;
    constexpr std::size_t steps_per_year = 12;
    constexpr std::size_t num_steps = (num_years * steps_per_year) + 1;
    constexpr double time_step = 1.0 / static_cast<double>(steps_per_year);

    constexpr std::size_t simd_width = xva::engine::MonteCarloEngine<>::simd_f64::size();
    if (config.num_paths % simd_width != 0)
    {
      config.num_paths = (config.num_paths / simd_width + 1) * simd_width;
      if (config.verbose)
      {
        std::println(stderr, "[WARN] Path count adjusted to {} for SIMD alignment", config.num_paths);
      }
    }

    std::vector<double> time_grid(num_steps);
    for (std::size_t i = 0; i < num_steps; ++i)
    {
      time_grid[i] = static_cast<double>(i) * time_step;
    }

    if (config.verbose)
    {
      std::println("[INFO] Allocating memory...");
    }
    xva::core::NPVMesh rate_mesh(num_steps, config.num_paths);
    xva::core::NPVMesh mtm_mesh(num_steps, config.num_paths);

    constexpr double initial_rate = 0.03;
    constexpr xva::models::HullWhite1FParams hw_params{ .mean_reversion = 0.05,
                                                        .long_term_mean = 0.04,
                                                        .volatility = 0.015 };
    const xva::models::HullWhite1F<> hw_model(hw_params, time_step);
    const xva::models::HW1FYieldCurve<> yield_curve(hw_params, initial_rate);

    const xva::instruments::InterestRateSwapTerms swap_terms{ .type = xva::instruments::SwapType::Receiver,
                                                              .notional = config.notional,
                                                              .fixed_rate = 0.035 };
    const xva::instruments::InterestRateSwap swap(swap_terms, { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0 });

    const xva::engine::MonteCarloEngine<> engine;
    const xva::pricing::InterestRateSwapPricer<> pricer;
    const xva::aggregation::CvaAggregator<> aggregator;

    if (config.verbose)
    {
      std::println("[INFO] Simulating stochastic paths...");
    }
    const auto time0 = std::chrono::high_resolution_clock::now();
    engine.generate_paths(rate_mesh, initial_rate, hw_model, config.seed);

    if (config.verbose)
    {
      std::println("[INFO] Pricing instruments...");
    }
    const auto time1 = std::chrono::high_resolution_clock::now();
    pricer.calculate_mtm(rate_mesh, time_grid, swap, yield_curve, mtm_mesh);

    if (config.verbose)
    {
      std::println("[INFO] Aggregating risk metrics...");
    }
    const auto time2 = std::chrono::high_resolution_clock::now();
    constexpr xva::aggregation::CounterpartyCreditProfile base_profile{ .recovery_rate = 0.40, .hazard_rate = 0.02 };
    static_cast<void>(aggregator.calculate(mtm_mesh, time_grid, base_profile));
    const auto time3 = std::chrono::high_resolution_clock::now();

    if (config.verbose)
    {
      const auto ms_sim = std::chrono::duration_cast<std::chrono::milliseconds>(time1 - time0).count();
      const auto ms_prc = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1).count();
      const auto ms_agg = std::chrono::duration_cast<std::chrono::milliseconds>(time3 - time2).count();
      print_performance_report(ms_sim, ms_prc, ms_agg);
    }

    print_risk_report(config, mtm_mesh, time_grid);
  }

}  // namespace

auto main(int argc, char* argv[]) -> int
{
  try
  {
    const std::span<const char* const> args{ argv, static_cast<std::size_t>(argc) };
    AppConfig config;
    if (!parse_arguments(args, config))
    {
      return 0;
    }
    run_simulation(config);
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "\n[ERROR] Fatal exception: " << e.what() << "\n";
    return 1;
  }
  catch (...)
  {
    std::cerr << "\n[ERROR] Unknown fatal exception caught\n";
    return 1;
  }
}
