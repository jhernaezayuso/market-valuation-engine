/// @file philox_rng.hpp
/// @brief SIMD-vectorized implementation of the Philox 4x32 counter-based PRNG.

#pragma once

// xva
#include "xva/core/simd_config.hpp"

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <experimental/simd>
#include <utility>

namespace xva::math
{

  /// @class Philox4x32
  /// @brief Counter-based Pseudo-Random Number Generator suitable for parallel computing.
  /// @tparam SimdWidth The width of the SIMD register.
  template <std::size_t SimdWidth = core::default_simd_width> class Philox4x32
  {
   public:
    /// @brief Standard iteration count for the Philox algorithm (10 rounds is standard).
    static constexpr std::size_t rounds = 10;

    /// @brief Philox constants for the substitution step.
    static constexpr uint32_t multiplier_0 = 0xD2'51'1F'53;
    static constexpr uint32_t multiplier_1 = 0xCD'9E'8D'57;

    /// @brief Weyl constants for the permutation key update.
    static constexpr uint32_t weyl_0 = 0x9E'37'79'B9;
    static constexpr uint32_t weyl_1 = 0xBB'67'AE'85;

    using simd_u32 = std::experimental::fixed_size_simd<uint32_t, SimdWidth>;

    /// @brief One 64-bit lane per 32-bit lane, holding the products of the widening multiplication.
    using simd_u64_wide = std::experimental::fixed_size_simd<uint64_t, SimdWidth>;

    /// @brief Holds the vectorized output of one Philox generation cycle.
    struct Result
    {
      std::array<simd_u32, 4> values;
    };

    /// @brief Constructs the generator with a specific 64-bit key split into two 32-bit seeds.
    /// @param seed_0 The lower 32 bits of the key.
    /// @param seed_1 The upper 32 bits of the key.
    explicit Philox4x32(uint32_t seed_0, uint32_t seed_1 = 0) : key_{ seed_0, seed_1 }
    {
    }

   private:
    /// @brief Performs a 32x32 -> 64 bit wide multiplication and splits the high/low bits.
    /// @param val_a The first operand (vectorized).
    /// @param val_b The second operand (scalar multiplier).
    /// @return A pair containing the high 32-bits and low 32-bits of the result.
    /// @details Both operands fit in 32 bits, which lets a compiler lower the product onto the
    /// widening multiply instead of emulating a full 64-bit one.
    [[nodiscard]] static auto multiply_hi_lo(const simd_u32& val_a, uint32_t val_b) -> std::pair<simd_u32, simd_u32>
    {
      constexpr unsigned int shift_amount = 32U;
      constexpr uint64_t low_word_mask = 0xFFFF'FFFFULL;

      const auto widened = std::experimental::static_simd_cast<simd_u64_wide>(val_a);
      const simd_u64_wide product = widened * static_cast<uint64_t>(val_b);

      const auto low = std::experimental::static_simd_cast<simd_u32>(product & low_word_mask);
      const auto high = std::experimental::static_simd_cast<simd_u32>(product >> shift_amount);

      return { high, low };
    }

   public:
    /// @brief Generates a vector of random numbers based on a given 4-element counter.
    /// @param counter An array of 4 vectorized 32-bit integers representing the generation index.
    /// @return The resulting pseudo-random bits encapsulated in a Result struct.
    [[nodiscard]] auto generate(const std::array<simd_u32, 4>& counter) const -> Result
    {
      simd_u32 state_0 = counter[0];
      simd_u32 state_1 = counter[1];
      simd_u32 state_2 = counter[2];
      simd_u32 state_3 = counter[3];

      simd_u32 key_0 = key_[0];
      simd_u32 key_1 = key_[1];

      for (std::size_t idx = 0; idx < rounds; ++idx)
      {
        auto [high_0, low_0] = multiply_hi_lo(state_0, multiplier_0);
        auto [high_1, low_1] = multiply_hi_lo(state_2, multiplier_1);

        state_0 = high_1 ^ state_1 ^ key_0;
        state_1 = low_1;
        state_2 = high_0 ^ state_3 ^ key_1;
        state_3 = low_0;

        key_0 += weyl_0;
        key_1 += weyl_1;
      }

      return { { state_0, state_1, state_2, state_3 } };
    }

   private:
    std::array<uint32_t, 2> key_{ 0, 0 };
  };

}  // namespace xva::math
