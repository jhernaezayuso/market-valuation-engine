/// @file philox_rng_test.cpp
/// @brief Unit tests for the Philox 4x32 vectorized pseudo-random number generator.

// xva
#include "xva/math/philox_rng.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>

namespace xva::math::test
{

  /// @brief Verifies deterministic behavior ensuring identical seeds/counters yield identical outputs.
  TEST(Philox4x32Test, Reproducibility)
  {
    constexpr uint32_t key_0 = 0x12'34'56'78;
    constexpr uint32_t key_1 = 0x9A'BC'DE'F0;
    constexpr std::size_t counter_size = 4;

    const Philox4x32<> rng1(key_0, key_1);
    const Philox4x32<> rng2(key_0, key_1);

    using simd_u32 = Philox4x32<>::simd_u32;
    const std::array<simd_u32, counter_size> counter = { 0, 0, 0, 0 };

    const auto res1 = rng1.generate(counter);
    const auto res2 = rng2.generate(counter);

    for (std::size_t i = 0; i < counter_size; ++i)
    {
      for (std::size_t lane = 0; lane < simd_u32::size(); ++lane)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(res1.values[i][lane], res2.values[i][lane]);
      }
    }
  }

  /// @brief Ensures the generator does not produce immediate collisions across distinct SIMD lanes.
  TEST(Philox4x32Test, IndependenceAcrossLanes)
  {
    constexpr uint32_t seed = 42;
    constexpr std::size_t counter_size = 4;
    constexpr uint32_t lane_multiplier = 100;

    const Philox4x32<> rng(seed);
    using simd_u32 = Philox4x32<>::simd_u32;
    std::array<simd_u32, counter_size> counter{};

    for (std::size_t i = 0; i < counter_size; ++i)
    {
      for (std::size_t lane = 0; lane < simd_u32::size(); ++lane)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        counter[i][lane] = static_cast<uint32_t>(lane + (i * lane_multiplier));
      }
    }

    const auto res = rng.generate(counter);
    std::set<uint32_t> seen;

    for (const auto& res_val : res.values)
    {
      for (std::size_t lane = 0; lane < simd_u32::size(); ++lane)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const uint32_t val = res_val[lane];
        EXPECT_FALSE(seen.contains(val));
        seen.insert(val);
      }
    }
  }

}  // namespace xva::math::test
