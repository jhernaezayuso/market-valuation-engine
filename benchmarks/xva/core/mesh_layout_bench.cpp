/// @file mesh_layout_bench.cpp
/// @brief Compares the step major Structure of Arrays mesh against an equivalent path major layout.

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/math/inverse_cdf.hpp"
#include "xva/math/philox_rng.hpp"
#include "xva/models/hull_white_1f.hpp"

// tbb
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

// google benchmark
#include <benchmark/benchmark.h>

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <experimental/simd>
#include <vector>

namespace xva::benchmarks
{

  namespace
  {

    /// @brief Register width and mesh dimensions shared by both layouts.
    constexpr std::size_t simd_width = 4;
    constexpr std::size_t num_steps = 256;
    constexpr std::size_t num_paths = 8'192;

    /// @brief Model setup of the simulated short rate.
    constexpr double initial_rate = 0.03;
    constexpr double mean_reversion = 0.05;
    constexpr double volatility = 0.01;
    constexpr double time_step = 0.02;
    constexpr uint32_t seed = 42;

    /// @brief Conversion constants mirroring those of the Monte Carlo engine.
    constexpr double u32_to_double_divisor = 4294967296.0;
    constexpr double u32_to_unit_offset = 0.5;
    constexpr std::size_t cache_line_alignment = 64;

    using simd_f64 = std::experimental::fixed_size_simd<double, simd_width>;
    using simd_u32 = math::Philox4x32<simd_width>::simd_u32;

    /// @brief Builds the model parameters used across the file.
    auto model_params() -> models::HullWhite1FParams
    {
      return models::HullWhite1FParams{ .mean_reversion = mean_reversion,
                                        .long_term_mean = mean_reversion * initial_rate,
                                        .volatility = volatility };
    }

    /// @brief Draws one vector of standard normal variates for a block of paths at a time step.
    auto draw_normal_variates(const math::Philox4x32<simd_width>& rng, const simd_u32& counter_0, std::size_t step_idx)
        -> simd_f64
    {
      const std::array<simd_u32, 4> counter = { counter_0, static_cast<uint32_t>(step_idx), 0, 0 };
      const auto rng_result = rng.generate(counter);

      alignas(cache_line_alignment) std::array<uint32_t, simd_width> random_ints{};
      rng_result.values[0].copy_to(random_ints.data(), std::experimental::element_aligned);

      alignas(cache_line_alignment) std::array<double, simd_width> uniform_doubles{};
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
      for (std::size_t lane = 0; lane < simd_width; ++lane)
      {
        uniform_doubles[lane] = (static_cast<double>(random_ints[lane]) + u32_to_unit_offset) / u32_to_double_divisor;
      }
      // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

      simd_f64 uniform_simd;
      uniform_simd.copy_from(uniform_doubles.data(), std::experimental::element_aligned);

      return math::NormalICDF<simd_width>::transform(uniform_simd);
    }

    /// @brief Builds the vectorized path indices that seed the Philox counter for a block.
    auto block_counter(std::size_t path_idx) -> simd_u32
    {
      alignas(cache_line_alignment) std::array<uint32_t, simd_width> path_indices{};
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
      for (std::size_t lane = 0; lane < simd_width; ++lane)
      {
        path_indices[lane] = static_cast<uint32_t>(path_idx + lane);
      }
      // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

      simd_u32 counter_0;
      counter_0.copy_from(path_indices.data(), std::experimental::element_aligned);
      return counter_0;
    }

    /// @brief Simulates into a step major buffer, the layout used by the engine.
    void generate_step_major(core::NPVMesh& mesh, const models::HullWhite1F<simd_width>& model)
    {
      const math::Philox4x32<simd_width> rng(seed);
      const std::size_t num_blocks = num_paths / simd_width;

      // NOLINTNEXTLINE(misc-include-cleaner)
      tbb::parallel_for(tbb::blocked_range<std::size_t>(0, num_blocks),
                        [&](const tbb::blocked_range<std::size_t>& range) -> void
                        {
                          for (std::size_t block_idx = range.begin(); block_idx < range.end(); ++block_idx)
                          {
                            const std::size_t path_idx = block_idx * simd_width;
                            const simd_u32 counter_0 = block_counter(path_idx);

                            simd_f64 current_values(initial_rate);
                            current_values.copy_to(&mesh.data()(0, path_idx), std::experimental::element_aligned);

                            for (std::size_t step_idx = 1; step_idx < num_steps; ++step_idx)
                            {
                              const simd_f64 shocks = draw_normal_variates(rng, counter_0, step_idx);
                              current_values = model.step(current_values, shocks);
                              current_values.copy_to(&mesh.data()(step_idx, path_idx),
                                                     std::experimental::element_aligned);
                            }
                          }
                        });
    }

    /// @brief Simulates into a path major buffer, the layout a per-scenario object design produces.
    void generate_path_major(std::vector<double>& buffer, const models::HullWhite1F<simd_width>& model)
    {
      const math::Philox4x32<simd_width> rng(seed);
      const std::size_t num_blocks = num_paths / simd_width;

      // NOLINTNEXTLINE(misc-include-cleaner)
      tbb::parallel_for(tbb::blocked_range<std::size_t>(0, num_blocks),
                        [&](const tbb::blocked_range<std::size_t>& range) -> void
                        {
                          for (std::size_t block_idx = range.begin(); block_idx < range.end(); ++block_idx)
                          {
                            const std::size_t path_idx = block_idx * simd_width;
                            const simd_u32 counter_0 = block_counter(path_idx);

                            simd_f64 current_values(initial_rate);
                            for (std::size_t lane = 0; lane < simd_width; ++lane)
                            {
                              buffer[((path_idx + lane) * num_steps)] = current_values[lane];
                            }

                            for (std::size_t step_idx = 1; step_idx < num_steps; ++step_idx)
                            {
                              const simd_f64 shocks = draw_normal_variates(rng, counter_0, step_idx);
                              current_values = model.step(current_values, shocks);

                              for (std::size_t lane = 0; lane < simd_width; ++lane)
                              {
                                buffer[((path_idx + lane) * num_steps) + step_idx] = current_values[lane];
                              }
                            }
                          }
                        });
    }

    /// @brief Sums the positive part of every value of a step major mesh.
    auto reduce_step_major(const core::NPVMesh& mesh) -> double
    {
      double total = 0.0;

      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        const auto step_view = mesh.data().get_step_view(step_idx);
        for (const double value : step_view)
        {
          total += value > 0.0 ? value : 0.0;
        }
      }

      return total;
    }

    /// @brief Sums the positive part of every value of a path major buffer.
    auto reduce_path_major(const std::vector<double>& buffer) -> double
    {
      double total = 0.0;

      for (std::size_t step_idx = 0; step_idx < num_steps; ++step_idx)
      {
        for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
        {
          const double value = buffer[(path_idx * num_steps) + step_idx];
          total += value > 0.0 ? value : 0.0;
        }
      }

      return total;
    }

    /// @brief Measures path generation into the step major mesh.
    void bm_generate_step_major(benchmark::State& state)
    {
      const models::HullWhite1F<simd_width> model(model_params(), time_step);
      core::NPVMesh mesh(num_steps, num_paths);

      for ([[maybe_unused]] const auto iteration : state)
      {
        generate_step_major(mesh, model);
        benchmark::DoNotOptimize(mesh.data().raw_data());
        benchmark::ClobberMemory();
      }

      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths));
    }

    /// @brief Measures path generation into the path major buffer.
    void bm_generate_path_major(benchmark::State& state)
    {
      const models::HullWhite1F<simd_width> model(model_params(), time_step);
      std::vector<double> buffer(num_steps * num_paths, 0.0);

      for ([[maybe_unused]] const auto iteration : state)
      {
        generate_path_major(buffer, model);
        benchmark::DoNotOptimize(buffer.data());
        benchmark::ClobberMemory();
      }

      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths));
    }

    /// @brief Measures the exposure reduction over the step major mesh.
    void bm_reduce_step_major(benchmark::State& state)
    {
      const models::HullWhite1F<simd_width> model(model_params(), time_step);
      core::NPVMesh mesh(num_steps, num_paths);
      generate_step_major(mesh, model);

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(reduce_step_major(mesh));
      }

      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths));
      state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths * sizeof(double)));
    }

    /// @brief Measures the exposure reduction over the path major buffer.
    void bm_reduce_path_major(benchmark::State& state)
    {
      const models::HullWhite1F<simd_width> model(model_params(), time_step);
      std::vector<double> buffer(num_steps * num_paths, 0.0);
      generate_path_major(buffer, model);

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(reduce_path_major(buffer));
      }

      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths));
      state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_steps * num_paths * sizeof(double)));
    }

    BENCHMARK(bm_generate_step_major)->Unit(benchmark::kMillisecond);
    BENCHMARK(bm_generate_path_major)->Unit(benchmark::kMillisecond);
    BENCHMARK(bm_reduce_step_major)->Unit(benchmark::kMillisecond);
    BENCHMARK(bm_reduce_path_major)->Unit(benchmark::kMillisecond);

  }  // namespace

}  // namespace xva::benchmarks
