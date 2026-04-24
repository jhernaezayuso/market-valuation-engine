/// @file aligned_allocator_test.cpp
/// @brief Unit tests for the custom aligned memory allocator.

// xva
#include "xva/core/aligned_allocator.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace xva::core::test
{

  /// @brief Verifies fundamental allocation and deallocation logic without memory leaks.
  TEST(AlignedAllocatorTest, AllocatesAndDeallocatesSuccessfully)
  {
    AlignedAllocator<double> allocator;
    constexpr std::size_t element_count = 100;

    auto* ptr = allocator.allocate(element_count);

    EXPECT_NE(ptr, nullptr);

    allocator.deallocate(ptr, element_count);
  }

  /// @brief Ensures returned pointers conform strictly to the specified cache line alignment.
  TEST(AlignedAllocatorTest, MemoryIsCorrectlyAligned)
  {
    AlignedAllocator<double> allocator;
    constexpr std::size_t element_count = 50;

    auto* ptr = allocator.allocate(element_count);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);

    EXPECT_EQ(address % default_cache_line_size, 0);

    allocator.deallocate(ptr, element_count);
  }

  /// @brief Validates compatibility with standard library containers like std::vector.
  TEST(AlignedAllocatorTest, IntegratesWithStdVector)
  {
    constexpr std::size_t element_count = 1'000;
    std::vector<double, AlignedAllocator<double>> vec(element_count);

    EXPECT_EQ(vec.size(), element_count);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto address = reinterpret_cast<std::uintptr_t>(vec.data());

    EXPECT_EQ(address % default_cache_line_size, 0);
  }

  /// @brief Checks that requesting size beyond limits throws appropriate standard exceptions.
  TEST(AlignedAllocatorTest, ThrowsOnInvalidAllocationSize)
  {
    AlignedAllocator<double> allocator;

    constexpr auto huge_size = std::numeric_limits<std::size_t>::max();

    EXPECT_THROW(static_cast<void>(allocator.allocate(huge_size)), std::bad_array_new_length);
  }

  /// @brief Confirms behavior when physical memory is exhausted.
  TEST(AlignedAllocatorTest, ThrowsBadAllocOnOutOfMemory)
  {
#ifndef NDEBUG
    AlignedAllocator<double> allocator;

    constexpr auto max_size = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t massive_size = max_size / sizeof(double) - 1'024;

    EXPECT_THROW(static_cast<void>(allocator.allocate(massive_size)), std::bad_alloc);
#else
    GTEST_SKIP() << "Skipped because out of memory is optimized in release mode";
#endif
  }

}  // namespace xva::core::test
