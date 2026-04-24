/// @file assert.hpp
/// @brief Performance aware and coverage friendly assertion macros.

#pragma once

#if !defined(XVA_ENABLE_COVERAGE_MACRO) && !defined(NDEBUG)
#include <cassert>
#endif

namespace xva::core
{

  /// @def XVA_ASSERT
  /// @brief Custom assertion macro adapted for test coverage and release optimizations.
  ///
  /// Behavior varies based on build configuration:
  /// - Coverage enabled: Throws std::logic_error to allow coverage tools to map the false branch.
  /// - Debug: Falls back to standard assert() to break in the debugger.
  /// - Release: Uses [[assume]] attribute to optimize compiler vectorization without branching.
#ifdef XVA_ENABLE_COVERAGE_MACRO
  // NOLINTBEGIN(cppcoreguidelines-avoid-do-while)
  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define XVA_ASSERT(expr)                                   \
  do                                                       \
  {                                                        \
    if (!(expr)) [[unlikely]]                              \
    {                                                      \
      throw std::logic_error("XVA_ASSERT failed: " #expr); \
    }                                                      \
  } while (false)
  // NOLINTEND(cppcoreguidelines-avoid-do-while)
#elifndef NDEBUG
  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define XVA_ASSERT(expr) assert(expr)
#else
  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define XVA_ASSERT(expr) [[assume(expr)]]
#endif

}  // namespace xva::core
