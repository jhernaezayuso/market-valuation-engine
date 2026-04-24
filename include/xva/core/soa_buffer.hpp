/// @file soa_buffer.hpp
/// @brief Contiguous memory buffer utilizing Structure of Arrays (SoA) data layout.

#pragma once

// xva
#include "xva/core/aligned_allocator.hpp"
#include "xva/core/assert.hpp"

// std
#include <cstddef>
#include <experimental/mdspan>
#include <span>
#include <vector>

namespace xva::core
{

  /// @class SoABuffer
  /// @brief 2D matrix buffer mapped to 1D contiguous aligned memory.
  /// @tparam T The numeric type to store (defaults to double).
  template <typename T = double> class SoABuffer
  {
   public:
    using Allocator = AlignedAllocator<T, default_cache_line_size>;

    using Extents =
        std::experimental::extents<std::size_t, std::experimental::dynamic_extent, std::experimental::dynamic_extent>;
    using MatrixView = std::experimental::mdspan<T, Extents, std::experimental::layout_right>;
    using ConstMatrixView = std::experimental::mdspan<const T, Extents, std::experimental::layout_right>;

    /// @brief Default constructor.
    SoABuffer() = default;

    /// @brief Initializes the buffer with the given dimensions.
    /// @param num_steps The first dimension.
    /// @param num_paths The second dimension.
    SoABuffer(std::size_t num_steps, std::size_t num_paths)
        : num_steps_(num_steps), num_paths_(num_paths), data_(num_steps * num_paths)
    {
    }

    /// @brief Mutable element access with 2D coordinates.
    /// @param step_idx The row index.
    /// @param path_idx The column index.
    /// @return A reference to the specified element.
    [[nodiscard]] auto operator()(std::size_t step_idx, std::size_t path_idx) -> T&
    {
      const MatrixView view(data_.data(), num_steps_, num_paths_);
      return view[step_idx, path_idx];
    }

    /// @brief Constant element access with 2D coordinates.
    /// @param step_idx The row index.
    /// @param path_idx The column index.
    /// @return A constant reference to the specified element.
    [[nodiscard]] auto operator()(std::size_t step_idx, std::size_t path_idx) const -> const T&
    {
      const ConstMatrixView view(data_.data(), num_steps_, num_paths_);
      return view[step_idx, path_idx];
    }

    /// @brief Retrieves a 1D span representing an entire time step across all paths.
    /// @param step_idx The time step index.
    /// @return A span covering n paths for the given step.
    [[nodiscard]] auto get_step_view(std::size_t step_idx) -> std::span<T>
    {
      XVA_ASSERT(step_idx < num_steps_);  // NOLINT(cppcoreguidelines-avoid-do-while)
      return std::span<T>(&data_[step_idx * num_paths_], num_paths_);
    }

    /// @brief Retrieves a constant 1D span representing an entire time step across all paths.
    /// @param step_idx The time step index.
    /// @return A constant span covering n paths for the given step.
    [[nodiscard]] auto get_step_view(std::size_t step_idx) const -> std::span<const T>
    {
      XVA_ASSERT(step_idx < num_steps_);  // NOLINT(cppcoreguidelines-avoid-do-while)
      return std::span<const T>(&data_[step_idx * num_paths_], num_paths_);
    }

    /// @brief Returns the number of allocated steps.
    [[nodiscard]] auto num_steps() const -> std::size_t
    {
      return num_steps_;
    }

    /// @brief Returns the number of allocated paths.
    [[nodiscard]] auto num_paths() const -> std::size_t
    {
      return num_paths_;
    }

    /// @brief Returns a pointer to the raw contiguous underlying memory.
    [[nodiscard]] auto raw_data() -> T*
    {
      return data_.data();
    }

    /// @brief Returns a constant pointer to the raw contiguous underlying memory.
    [[nodiscard]] auto raw_data() const -> const T*
    {
      return data_.data();
    }

   private:
    std::size_t num_steps_{ 0 };
    std::size_t num_paths_{ 0 };
    std::vector<T, Allocator> data_;
  };

}  // namespace xva::core
