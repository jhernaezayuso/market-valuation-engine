/// @file simd_config.hpp
/// @brief Build-wide selection of the SIMD register width the engine is compiled for.

#pragma once

// std
#include <cstddef>

namespace xva::core
{

  /// @brief Number of double precision lanes every component of the pipeline operates on.
#ifdef XVA_SIMD_WIDTH
  inline constexpr std::size_t default_simd_width = XVA_SIMD_WIDTH;
#else
  inline constexpr std::size_t default_simd_width = 4;
#endif

  /// @brief Multiple that a requested number of paths is rounded up to, independent of the width.
  inline constexpr std::size_t path_count_granularity = 64;

  static_assert(path_count_granularity % default_simd_width == 0,
                "The path count granularity must be a multiple of the SIMD width.");

}  // namespace xva::core
