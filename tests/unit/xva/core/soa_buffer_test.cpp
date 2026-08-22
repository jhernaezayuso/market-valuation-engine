/// @file soa_buffer_test.cpp
/// @brief Unit tests validating memory layout and view abstractions of SoABuffer.

// xva
#include "xva/core/soa_buffer.hpp"
#include "xva/core/aligned_allocator.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace xva::core::test
{

  /// @brief Validates 2D dimension tracking inside the buffer.
  TEST(SoABufferTest, InitializationAndSizing)
  {
    constexpr std::size_t num_steps = 100;
    constexpr std::size_t num_paths = 1'000;

    const SoABuffer<double> buffer(num_steps, num_paths);

    EXPECT_EQ(buffer.num_steps(), num_steps);
    EXPECT_EQ(buffer.num_paths(), num_paths);
  }

  /// @brief Ensures 2D coordinate operations correctly map to the underlying 1D data.
  TEST(SoABufferTest, ElementReadWriteAccess)
  {
    constexpr std::size_t num_steps = 10;
    constexpr std::size_t num_paths = 50;
    constexpr std::size_t target_step = 2;
    constexpr std::size_t target_path = 25;

    SoABuffer<double> buffer(num_steps, num_paths);

    buffer(target_step, target_path) = std::numbers::pi;

    EXPECT_DOUBLE_EQ(buffer(target_step, target_path), std::numbers::pi);
  }

  /// @brief Confirms that std::span views accurately abstract horizontal steps (row equivalents).
  TEST(SoABufferTest, StepViewProvidesCorrectSpan)
  {
    constexpr std::size_t num_steps = 5;
    constexpr std::size_t num_paths = 10;
    constexpr std::size_t target_step = 2;
    constexpr double multiplier = 2.0;
    constexpr double expected_0 = 0.0;
    constexpr double expected_last = 18.0;

    SoABuffer<double> buffer(num_steps, num_paths);

    for (std::size_t path_idx = 0; path_idx < num_paths; ++path_idx)
    {
      buffer(target_step, path_idx) = static_cast<double>(path_idx) * multiplier;
    }

    auto view = buffer.get_step_view(target_step);

    EXPECT_EQ(view.size(), num_paths);
    EXPECT_DOUBLE_EQ(view[0], expected_0);
    EXPECT_DOUBLE_EQ(view[num_paths - 1], expected_last);
  }

  /// @brief Ensures immutability contracts are maintained through const references.
  TEST(SoABufferTest, ConstStepViewWorks)
  {
    constexpr std::size_t num_steps = 2;
    constexpr std::size_t num_paths = 2;
    constexpr std::size_t target_step = 0;
    constexpr std::size_t target_path = 1;
    constexpr double expected_value = 99.9;

    SoABuffer<double> buffer(num_steps, num_paths);
    buffer(target_step, target_path) = expected_value;

    const auto& const_buffer = buffer;
    auto view = const_buffer.get_step_view(target_step);

    EXPECT_DOUBLE_EQ(view[target_path], expected_value);
  }

  /// @brief Verifies integration with AlignedAllocator ensuring underlying contiguity respects boundaries.
  TEST(SoABufferTest, UnderlyingDataIsAlignedTo64Bytes)
  {
    constexpr std::size_t num_steps = 50;
    constexpr std::size_t num_paths = 100;

    SoABuffer<double> buffer(num_steps, num_paths);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto address = reinterpret_cast<std::uintptr_t>(buffer.raw_data());

    EXPECT_EQ(address % default_cache_line_size, 0);
  }

  /// @brief Confirms robust assertion handling during illegal memory access operations.
  TEST(SoABufferTest, AssertsOnOutOfBoundsAccess)
  {
    constexpr std::size_t num_steps = 5;
    constexpr std::size_t num_paths = 10;

    SoABuffer<double> buffer(num_steps, num_paths);

#ifdef XVA_ENABLE_COVERAGE_MACRO
    constexpr std::size_t out_of_bounds_step1 = 5;
    constexpr std::size_t out_of_bounds_step2 = 99;

    EXPECT_THROW(static_cast<void>(buffer.get_step_view(out_of_bounds_step1)), std::logic_error);
    EXPECT_THROW(static_cast<void>(std::as_const(buffer).get_step_view(out_of_bounds_step2)), std::logic_error);
#elifndef NDEBUG
    constexpr std::size_t out_of_bounds_step1 = 5;
    constexpr std::size_t out_of_bounds_step2 = 99;

    EXPECT_DEATH(static_cast<void>(buffer.get_step_view(out_of_bounds_step1)), "");
    EXPECT_DEATH(static_cast<void>(std::as_const(buffer).get_step_view(out_of_bounds_step2)), "");
#else
    GTEST_SKIP() << "Skipped because out of bounds is undefined behavior in release mode";
#endif
  }

}  // namespace xva::core::test
