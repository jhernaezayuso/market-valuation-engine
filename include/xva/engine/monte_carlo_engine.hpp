/// @file monte_carlo_engine.hpp
/// @brief Orchestrator for parallel Monte Carlo path generation using Intel TBB and SIMD.

#pragma once

// xva
#include "xva/core/npv_mesh.hpp"
#include "xva/core/simd_config.hpp"
#include "xva/math/inverse_cdf.hpp"
#include "xva/math/philox_rng.hpp"

// tbb
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

// std
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <experimental/simd>
#include <stdexcept>

namespace xva::engine
{

  /// @concept IsStochasticModel
  /// @brief Enforces the static interface required for a market model in the Monte Carlo engine.
  /// @details The model must provide a step() method that takes current values and normal variates
  /// (both as SIMD vectors) and returns the next simulated values as a SIMD vector.
  template <typename T, std::size_t SimdWidth>
  concept IsStochasticModel = requires(const T& model,
                                       const std::experimental::fixed_size_simd<double, SimdWidth>& values,
                                       const std::experimental::fixed_size_simd<double, SimdWidth>& shocks) {
    { model.step(values, shocks) } -> std::same_as<std::experimental::fixed_size_simd<double, SimdWidth>>;
  };

  /// @class MonteCarloEngine
  /// @brief Executes parallelized financial simulations to populate a Net Present Value mesh.
  template <std::size_t SimdWidth = core::default_simd_width> class MonteCarloEngine
  {
   public:
    using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;
    using simd_u32 = typename math::Philox4x32<SimdWidth>::simd_u32;

    static constexpr double u32_to_double_divisor = 4294967296.0;

    static constexpr double u32_to_unit_offset = 0.5;

    static constexpr std::size_t cache_line_alignment = 64;

    MonteCarloEngine() = default;

    /// @brief Generates asset price paths and populates the NPV mesh.
    /// @tparam StochasticModel The mathematical model type fulfilling IsStochasticModel.
    template <typename StochasticModel>
      requires IsStochasticModel<StochasticModel, SimdWidth>
    void generate_paths(core::NPVMesh& mesh, double spot_value, const StochasticModel& model, uint32_t seed) const
    {
      const std::size_t num_steps = mesh.data().num_steps();
      const std::size_t num_paths = mesh.data().num_paths();

      if (num_steps < 2) [[unlikely]]
      {
        throw std::invalid_argument("Monte Carlo simulation requires at least 2 time steps.");
      }

      if (num_paths % simd_f64::size() != 0) [[unlikely]]
      {
        throw std::invalid_argument("Number of paths must be an exact multiple of the SIMD register width.");
      }

      const math::Philox4x32<SimdWidth> rng(seed);
      const std::size_t num_blocks = num_paths / simd_f64::size();

      tbb::parallel_for(
          tbb::blocked_range<std::size_t>(0, num_blocks),  // GCOVR_EXCL_LINE
          [&](const tbb::blocked_range<std::size_t>& range) -> void
          {
            for (std::size_t block_idx = range.begin(); block_idx < range.end(); ++block_idx)
            {
              const std::size_t path_idx = block_idx * simd_f64::size();

              alignas(cache_line_alignment) std::array<uint32_t, SimdWidth> path_indices{};
              for (std::size_t i = 0; i < SimdWidth; ++i)
              {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                path_indices[i] = static_cast<uint32_t>(path_idx + i);
              }
              simd_u32 counter_0;
              counter_0.copy_from(path_indices.data(), std::experimental::element_aligned);

              simd_f64 current_values(spot_value);
              current_values.copy_to(&mesh.data()(0, path_idx), std::experimental::element_aligned);

              for (std::size_t step_idx = 1; step_idx < num_steps; ++step_idx)
              {
                const std::array<simd_u32, 4> counter = { counter_0, static_cast<uint32_t>(step_idx), 0, 0 };

                const auto rng_result = rng.generate(counter);

                alignas(cache_line_alignment) std::array<uint32_t, SimdWidth> random_ints{};
                rng_result.values[0].copy_to(random_ints.data(), std::experimental::element_aligned);

                alignas(cache_line_alignment) std::array<double, SimdWidth> uniform_doubles{};
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
                for (std::size_t i = 0; i < SimdWidth; ++i)
                {
                  uniform_doubles[i] =
                      (static_cast<double>(random_ints[i]) + u32_to_unit_offset) / u32_to_double_divisor;
                }
                // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

                simd_f64 uniform_simd;
                uniform_simd.copy_from(uniform_doubles.data(), std::experimental::element_aligned);

                const simd_f64 normal_simd = math::NormalICDF<SimdWidth>::transform(uniform_simd);
                const simd_f64 next_values = model.step(current_values, normal_simd);

                next_values.copy_to(&mesh.data()(step_idx, path_idx), std::experimental::element_aligned);
                current_values = next_values;
              }
            }
          });
    }
  };

}  // namespace xva::engine
