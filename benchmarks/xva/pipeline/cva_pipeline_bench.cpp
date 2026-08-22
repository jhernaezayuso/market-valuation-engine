/// @file cva_pipeline_bench.cpp
/// @brief Measures the full xVA pipeline and the cost of each of its stages in isolation.

// xva
#include "xva/aggregation/cva_aggregator.hpp"
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/instruments/interest_rate_swap.hpp"
#include "xva/models/hull_white_1f.hpp"
#include "xva/models/yield_curve.hpp"
#include "xva/pricing/interest_rate_swap_pricer.hpp"

// google benchmark
#include <benchmark/benchmark.h>

// std
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xva::benchmarks
{

  namespace
  {

    /// @brief Market and contract setup of the priced portfolio.
    constexpr double notional = 10'000'000.0;
    constexpr double initial_rate = 0.03;
    constexpr double fixed_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double volatility = 0.01;
    constexpr double maturity = 5.0;

    /// @brief Mesh dimensions, a monthly grid over the life of the swap.
    constexpr std::size_t num_steps = 61;
    constexpr std::size_t num_paths = 65'536;
    constexpr uint32_t seed = 42;

    /// @brief Payment dates of the swap, one per year including inception.
    constexpr std::size_t num_payment_dates = 6;

    /// @brief Credit parameters of the counterparty.
    constexpr double recovery_rate = 0.4;
    constexpr double hazard_rate = 0.02;

    /// @brief Builds the model parameters used across the file.
    auto model_params() -> models::HullWhite1FParams
    {
      return models::HullWhite1FParams{ .mean_reversion = mean_reversion,
                                        .long_term_mean = mean_reversion * initial_rate,
                                        .volatility = volatility };
    }

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

    /// @brief Builds the swap priced by every scenario in this file.
    auto benchmark_swap() -> instruments::InterestRateSwap
    {
      const instruments::InterestRateSwapTerms terms{ .type = instruments::SwapType::Payer,
                                                      .notional = notional,
                                                      .fixed_rate = fixed_rate };
      return { terms, annual_schedule() };
    }

    /// @brief Number of mesh cells touched by one pass of a stage.
    constexpr auto mesh_cells() -> int64_t
    {
      return static_cast<int64_t>(num_steps * num_paths);
    }

    /// @brief Measures diffusion in isolation.
    void bm_stage_path_generation(benchmark::State& state)
    {
      const models::HullWhite1F<> model(model_params(), maturity / static_cast<double>(num_steps - 1));
      const engine::MonteCarloEngine<> mc_engine;
      core::NPVMesh rate_mesh(num_steps, num_paths);

      for ([[maybe_unused]] const auto iteration : state)
      {
        mc_engine.generate_paths(rate_mesh, initial_rate, model, seed);
        benchmark::DoNotOptimize(rate_mesh.data().raw_data());
        benchmark::ClobberMemory();
      }

      state.SetItemsProcessed(state.iterations() * mesh_cells());
    }

    /// @brief Measures swap valuation in isolation over a pre-generated rate mesh.
    void bm_stage_swap_pricing(benchmark::State& state)
    {
      const double time_step = maturity / static_cast<double>(num_steps - 1);

      const models::HullWhite1F<> model(model_params(), time_step);
      const engine::MonteCarloEngine<> mc_engine;
      core::NPVMesh rate_mesh(num_steps, num_paths);
      mc_engine.generate_paths(rate_mesh, initial_rate, model, seed);

      const instruments::InterestRateSwap swap = benchmark_swap();
      const models::HW1FYieldCurve<> curve(model_params(), initial_rate);
      const std::vector<double> time_grid = simulation_time_grid();

      const pricing::InterestRateSwapPricer<> pricer;
      core::NPVMesh mtm_mesh(num_steps, num_paths);

      for ([[maybe_unused]] const auto iteration : state)
      {
        pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);
        benchmark::DoNotOptimize(mtm_mesh.data().raw_data());
        benchmark::ClobberMemory();
      }

      state.SetItemsProcessed(state.iterations() * mesh_cells());
    }

    /// @brief Measures exposure extraction and CVA integration over a pre-priced mesh.
    void bm_stage_cva_aggregation(benchmark::State& state)
    {
      const double time_step = maturity / static_cast<double>(num_steps - 1);

      const models::HullWhite1F<> model(model_params(), time_step);
      const engine::MonteCarloEngine<> mc_engine;
      core::NPVMesh rate_mesh(num_steps, num_paths);
      mc_engine.generate_paths(rate_mesh, initial_rate, model, seed);

      const instruments::InterestRateSwap swap = benchmark_swap();
      const models::HW1FYieldCurve<> curve(model_params(), initial_rate);
      const std::vector<double> time_grid = simulation_time_grid();

      const pricing::InterestRateSwapPricer<> pricer;
      core::NPVMesh mtm_mesh(num_steps, num_paths);
      pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);

      constexpr aggregation::CounterpartyCreditProfile profile{ .recovery_rate = recovery_rate,
                                                                .hazard_rate = hazard_rate };
      const aggregation::CvaAggregator<> aggregator;

      for ([[maybe_unused]] const auto iteration : state)
      {
        auto result = aggregator.calculate(mtm_mesh, time_grid, profile);
        benchmark::DoNotOptimize(result.cva_value);
      }

      state.SetItemsProcessed(state.iterations() * mesh_cells());
    }

    /// @brief Measures the complete pipeline from diffusion to credit charge.
    void bm_full_pipeline(benchmark::State& state)
    {
      const double time_step = maturity / static_cast<double>(num_steps - 1);

      const models::HullWhite1F<> model(model_params(), time_step);
      const engine::MonteCarloEngine<> mc_engine;
      const instruments::InterestRateSwap swap = benchmark_swap();
      const models::HW1FYieldCurve<> curve(model_params(), initial_rate);
      const std::vector<double> time_grid = simulation_time_grid();
      const pricing::InterestRateSwapPricer<> pricer;
      const aggregation::CvaAggregator<> aggregator;

      constexpr aggregation::CounterpartyCreditProfile profile{ .recovery_rate = recovery_rate,
                                                                .hazard_rate = hazard_rate };

      core::NPVMesh rate_mesh(num_steps, num_paths);
      core::NPVMesh mtm_mesh(num_steps, num_paths);

      for ([[maybe_unused]] const auto iteration : state)
      {
        mc_engine.generate_paths(rate_mesh, initial_rate, model, seed);
        pricer.calculate_mtm(rate_mesh, time_grid, swap, curve, mtm_mesh);
        auto result = aggregator.calculate(mtm_mesh, time_grid, profile);
        benchmark::DoNotOptimize(result.cva_value);
      }

      state.SetItemsProcessed(state.iterations() * mesh_cells());
    }

    BENCHMARK(bm_stage_path_generation)->Unit(benchmark::kMillisecond);
    BENCHMARK(bm_stage_swap_pricing)->Unit(benchmark::kMillisecond);
    BENCHMARK(bm_stage_cva_aggregation)->Unit(benchmark::kMillisecond);
    BENCHMARK(bm_full_pipeline)->Unit(benchmark::kMillisecond);

  }  // namespace

}  // namespace xva::benchmarks
