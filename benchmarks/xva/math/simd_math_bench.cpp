/// @file simd_math_bench.cpp
/// @brief Probes whether the standard library vectorizes transcendental functions on SIMD types.
///

// google benchmark
#include <benchmark/benchmark.h>

// std
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <experimental/simd>
#include <vector>

namespace xva::benchmarks
{

  namespace
  {

    /// @brief Working set sized to stay inside the level one data cache.
    constexpr std::size_t total_elements = 4'096;

    /// @brief Operand range, chosen so exponential, logarithm and square root are all well defined.
    constexpr double minimum_operand = 0.25;
    constexpr double operand_span = 1.0;

    /// @brief Builds the operand buffer shared by every case.
    auto make_operands() -> std::vector<double>
    {
      std::vector<double> operands(total_elements, 0.0);

      for (std::size_t idx = 0; idx < total_elements; ++idx)
      {
        const double position = static_cast<double>(idx) / static_cast<double>(total_elements);
        operands[idx] = minimum_operand + (operand_span * position);
      }

      return operands;
    }

    /// @brief Applies a vector kernel across the whole operand buffer.
    /// @tparam SimdWidth The width of the double precision SIMD register.
    /// @tparam Kernel The vector operation under measurement.
    template <std::size_t SimdWidth, typename Kernel>
    auto accumulate_vector_kernel(const std::vector<double>& operands, Kernel kernel) -> double
    {
      using simd_f64 = std::experimental::fixed_size_simd<double, SimdWidth>;

      simd_f64 accumulator = 0.0;

      for (std::size_t idx = 0; idx < total_elements; idx += SimdWidth)
      {
        simd_f64 value;
        value.copy_from(&operands[idx], std::experimental::element_aligned);
        accumulator += kernel(value);
      }

      return std::experimental::reduce(accumulator);
    }

    /// @brief Applies a scalar kernel across the whole operand buffer.
    /// @tparam Kernel The scalar operation under measurement.
    template <typename Kernel>
    auto accumulate_scalar_kernel(const std::vector<double>& operands, Kernel kernel) -> double
    {
      double accumulator = 0.0;

      for (std::size_t idx = 0; idx < total_elements; ++idx)
      {
        accumulator += kernel(operands[idx]);
      }

      return accumulator;
    }

    /// @brief Reports the width alongside the timing so the sweep reads as one curve.
    void report_width(benchmark::State& state, std::size_t simd_width)
    {
      state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total_elements));
      state.counters["simd_width"] = static_cast<double>(simd_width);
    }

    /// @brief The four operations under measurement, named so each case reads as one call.
    constexpr auto multiply_add_kernel = [](const auto& value) -> auto
    {
      return (value * value) + value;
    };
    constexpr auto sqrt_kernel = [](const auto& value) -> auto
    {
      return std::experimental::sqrt(value);
    };
    constexpr auto exp_kernel = [](const auto& value) -> auto
    {
      return std::experimental::exp(value);
    };
    constexpr auto log_kernel = [](const auto& value) -> auto
    {
      return std::experimental::log(value);
    };
    constexpr auto scalar_exp_kernel = [](double value) -> double
    {
      return std::exp(value);
    };
    constexpr auto scalar_log_kernel = [](double value) -> double
    {
      return std::log(value);
    };

    /// @brief Control case: plain arithmetic, which the compiler is expected to vectorize.
    template <std::size_t SimdWidth> void bm_simd_multiply_add(benchmark::State& state)
    {
      const std::vector<double> operands = make_operands();

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(accumulate_vector_kernel<SimdWidth>(operands, multiply_add_kernel));
      }

      report_width(state, SimdWidth);
    }

    /// @brief Control case: square root maps to a single vector instruction on this architecture.
    template <std::size_t SimdWidth> void bm_simd_sqrt(benchmark::State& state)
    {
      const std::vector<double> operands = make_operands();

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(accumulate_vector_kernel<SimdWidth>(operands, sqrt_kernel));
      }

      report_width(state, SimdWidth);
    }

    /// @brief Case under suspicion: the exponential used by the yield curve and the GBM model.
    template <std::size_t SimdWidth> void bm_simd_exp(benchmark::State& state)
    {
      const std::vector<double> operands = make_operands();

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(accumulate_vector_kernel<SimdWidth>(operands, exp_kernel));
      }

      report_width(state, SimdWidth);
    }

    /// @brief Case under suspicion: the logarithm used by the inverse normal CDF tails.
    template <std::size_t SimdWidth> void bm_simd_log(benchmark::State& state)
    {
      const std::vector<double> operands = make_operands();

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(accumulate_vector_kernel<SimdWidth>(operands, log_kernel));
      }

      report_width(state, SimdWidth);
    }

    /// @brief Scalar reference for the exponential.
    void bm_scalar_exp(benchmark::State& state)
    {
      const std::vector<double> operands = make_operands();

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(accumulate_scalar_kernel(operands, scalar_exp_kernel));
      }

      report_width(state, 0);
    }

    /// @brief Scalar reference for the logarithm.
    void bm_scalar_log(benchmark::State& state)
    {
      const std::vector<double> operands = make_operands();

      for ([[maybe_unused]] const auto iteration : state)
      {
        benchmark::DoNotOptimize(accumulate_scalar_kernel(operands, scalar_log_kernel));
      }

      report_width(state, 0);
    }

    BENCHMARK_TEMPLATE(bm_simd_multiply_add, 1);
    BENCHMARK_TEMPLATE(bm_simd_multiply_add, 2);
    BENCHMARK_TEMPLATE(bm_simd_multiply_add, 4);
    BENCHMARK_TEMPLATE(bm_simd_multiply_add, 8);

    BENCHMARK_TEMPLATE(bm_simd_sqrt, 1);
    BENCHMARK_TEMPLATE(bm_simd_sqrt, 2);
    BENCHMARK_TEMPLATE(bm_simd_sqrt, 4);
    BENCHMARK_TEMPLATE(bm_simd_sqrt, 8);

    BENCHMARK_TEMPLATE(bm_simd_exp, 1);
    BENCHMARK_TEMPLATE(bm_simd_exp, 2);
    BENCHMARK_TEMPLATE(bm_simd_exp, 4);
    BENCHMARK_TEMPLATE(bm_simd_exp, 8);

    BENCHMARK_TEMPLATE(bm_simd_log, 1);
    BENCHMARK_TEMPLATE(bm_simd_log, 2);
    BENCHMARK_TEMPLATE(bm_simd_log, 4);
    BENCHMARK_TEMPLATE(bm_simd_log, 8);

    BENCHMARK(bm_scalar_exp);
    BENCHMARK(bm_scalar_log);

  }  // namespace

}  // namespace xva::benchmarks
