/// @file aligned_allocator.hpp
/// @brief Custom memory allocator ensuring strict memory alignment.

#pragma once

// std
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace xva::core
{

  /// @brief Default size of a cache line on modern architectures.
  /// Used to prevent false sharing in concurrent environments.
  inline constexpr std::size_t default_cache_line_size = 64;

  /// @class AlignedAllocator
  /// @brief An STL-compatible allocator that aligns memory to a specified boundary.
  /// @tparam T The type of objects to allocate.
  /// @tparam Alignment The byte alignment boundary. Must be a power of 2.
  template <typename T, std::size_t Alignment = default_cache_line_size> struct AlignedAllocator
  {
    static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of 2");

    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    /// @brief Rebinds the allocator to another type.
    /// @tparam U The alternative type to allocate.
    template <class U> struct rebind
    {
      using other = AlignedAllocator<U, Alignment>;
    };

    /// @brief Default constructor.
    AlignedAllocator() noexcept = default;

    /// @brief Copy constructor for rebind support.
    /// @tparam U The type of the source allocator.
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    AlignedAllocator(const AlignedAllocator<U, Alignment>& /*other*/) noexcept
    {
    }

    /// @brief Allocates strictly aligned memory.
    /// @param count Number of elements of type T to allocate.
    /// @return Pointer to the newly allocated aligned memory block.
    /// @throws std::bad_array_new_length if requested size exceeds max numeric limits.
    /// @throws std::bad_alloc if the underlying memory allocation fails.
    [[nodiscard]] auto allocate(const std::size_t count) -> T*
    {
      if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) [[unlikely]]
      {
        throw std::bad_array_new_length();
      }

      const std::size_t total_bytes = count * sizeof(T);

      const std::size_t alloc_size = (total_bytes + Alignment - 1) & ~(Alignment - 1);

      auto* ptr =  // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc, hicpp-no-malloc)
          static_cast<T*>(std::aligned_alloc(Alignment, alloc_size));

      if (ptr != nullptr)
      {
        return ptr;
      }

      throw std::bad_alloc();
    }

    /// @brief Deallocates memory previously allocated by allocate().
    /// @param ptr Pointer to the aligned memory block.
    void deallocate(T* ptr, std::size_t /*count*/) noexcept
    {
      std::free(ptr);  // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory, hicpp-no-malloc)
    }

    /// @brief Compares two allocators for equality.
    template <typename U> auto operator==(const AlignedAllocator<U, Alignment>& /*other*/) const noexcept -> bool
    {
      return true;
    }

    /// @brief Compares two allocators for inequality.
    template <typename U> auto operator!=(const AlignedAllocator<U, Alignment>& /*other*/) const noexcept -> bool
    {
      return false;
    }
  };

}  // namespace xva::core
