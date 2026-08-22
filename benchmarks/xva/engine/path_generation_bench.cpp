/// @file path_generation_bench.cpp
/// @brief Measures how Monte Carlo path generation scales with thread count and SIMD width.

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/engine/monte_carlo_engine.hpp"
#include "xva/models/hull_white_1f.hpp"

// tbb
#include <tbb/task_arena.h>

// google benchmark
#include <benchmark/benchmark.h>

// std
#include <cstddef>
#include <cstdint>

namespace xva::benchmarks
{

  namespace
  {

    /// @brief Model setup of the simulated short rate.
    constexpr double initial_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double volatility = 0.01;
    constexpr double time_step = 0.02;
    constexpr uint32_t seed = 42;

    /// @brief Mesh dimensions, with a path count divisible by every SIMD width under test.
    constexpr std::size_t num_steps = 256;
    constexpr std::size_t num_paths = 16'384;

    /// @brief Thread counts swept by the scaling benchmark.
    constexpr int64_t min_thread_count = 1;
    constexpr int64_t max_thread_count = 16;

    /// @brief Builds the model parameters used across the file.
    auto model_params() -> models::HullWhite1FParams
    {
      return models::HullWhite1FParams{ .mean_reversion = mean_reversion,
                                        .long_term_mean = mean_reversion * initial_rate,
                                        .volatility = volatility };
    }

    /// @brief Measures generation throughput at an explicit degree of parallelism.
    void bm_path_generation_by_thread_count(benchmark::State& state)
    {
      const auto thread_count = static_cast<int>(state.range(0));

      // NOLINTNEXTLINE(misc-include-cleaner)
      tbb::task_arena arena(thread_count);
      arena.initialize();

      const models::HullWhite1F<> model(model_params(), time_step);
      const engine::MonteCarloEngine<> mc_engine;
      core::NPVMesh mesh(num_steps, num_paths);

      arena.execute(
          [&]() -> void
          {
            mc_engine.generate_paths(mesh, initial_rate, model, seed);
          });

      for ([[maybe_unused]] const auto iteration : state)
      {
        arena.execute(
            [&]() -> void
            {
              mc_engine.generate_paths(mesh, initial_rate, model, seed);
            });
        benchmark::DoNotOptimize(mesh.data().raw_data());
        benchmark::ClobberMemory();
      }

      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths));
      state.counters["threads"] = static_cast<double>(thread_count);
    }

    /// @brief Measures generation throughput at a fixed SIMD register width.
    /// @tparam SimdWidth The width of the double precision SIMD register.
    template <std::size_t SimdWidth> void bm_path_generation_by_simd_width(benchmark::State& state)
    {
      const models::HullWhite1F<SimdWidth> model(model_params(), time_step);
      const engine::MonteCarloEngine<SimdWidth> mc_engine;
      core::NPVMesh mesh(num_steps, num_paths);

      for ([[maybe_unused]] const auto iteration : state)
      {
        mc_engine.generate_paths(mesh, initial_rate, model, seed);
        benchmark::DoNotOptimize(mesh.data().raw_data());
        benchmark::ClobberMemory();
      }

      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths));
      state.counters["simd_width"] = static_cast<double>(SimdWidth);
    }

    BENCHMARK(bm_path_generation_by_thread_count)
        ->RangeMultiplier(2)
        ->Range(min_thread_count, max_thread_count)
        ->Unit(benchmark::kMillisecond)
        ->UseRealTime();

    BENCHMARK_TEMPLATE(bm_path_generation_by_simd_width, 1)->Unit(benchmark::kMillisecond);
    BENCHMARK_TEMPLATE(bm_path_generation_by_simd_width, 2)->Unit(benchmark::kMillisecond);
    BENCHMARK_TEMPLATE(bm_path_generation_by_simd_width, 4)->Unit(benchmark::kMillisecond);
    BENCHMARK_TEMPLATE(bm_path_generation_by_simd_width, 8)->Unit(benchmark::kMillisecond);

  }  // namespace

}  // namespace xva::benchmarks
