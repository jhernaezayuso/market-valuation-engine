/// @file npv_mesh.hpp
/// @brief Data structure representing the Net Present Value grid.

#pragma once

// xva
#include "xva/core/soa_buffer.hpp"

// std
#include <cstddef>

namespace xva::core
{

  /// @class NPVMesh
  /// @brief Financial domain wrapper around a SoABuffer to store simulated valuations.
  class NPVMesh
  {
   public:
    /// @brief Constructs a new NPV Mesh.
    /// @param num_steps The number of discrete time steps in the simulation.
    /// @param num_paths The number of Monte Carlo scenarios (paths).
    NPVMesh(std::size_t num_steps, std::size_t num_paths) : buffer_(num_steps, num_paths)
    {
    }

    /// @brief Returns a mutable reference to the underlying Structure of Arrays buffer.
    [[nodiscard]] auto data() -> SoABuffer<double>&
    {
      return buffer_;
    }

    /// @brief Returns a constant reference to the underlying Structure of Arrays buffer.
    [[nodiscard]] auto data() const -> const SoABuffer<double>&
    {
      return buffer_;
    }

   private:
    SoABuffer<double> buffer_;
  };

}  // namespace xva::core
