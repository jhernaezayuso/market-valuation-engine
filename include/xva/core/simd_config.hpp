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

}  // namespace xva::core
