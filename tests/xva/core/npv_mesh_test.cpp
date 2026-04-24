/// @file npv_mesh_test.cpp
/// @brief Unit tests for the Net Present Value domain model matrix.

// xva
#include "xva/core/npv_mesh.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>

namespace xva::core::test
{

  /// @brief Confirms the constructor correctly configures the internal buffer dimensions.
  TEST(NPVMeshTest, InitializationCreatesCorrectBufferSizes)
  {
    constexpr std::size_t num_steps = 120;
    constexpr std::size_t num_paths = 5'000;

    NPVMesh mesh(num_steps, num_paths);

    EXPECT_EQ(mesh.data().num_steps(), num_steps);
    EXPECT_EQ(mesh.data().num_paths(), num_paths);
  }

  /// @brief Verifies that values written to the mesh accurately persist in the delegated memory.
  TEST(NPVMeshTest, DataAccessIsCorrectlyDelegated)
  {
    constexpr std::size_t num_steps = 10;
    constexpr std::size_t num_paths = 100;
    constexpr std::size_t target_step = 5;
    constexpr std::size_t target_path = 42;
    constexpr double expected_value = 15000.50;

    NPVMesh mesh(num_steps, num_paths);

    mesh.data()(target_step, target_path) = expected_value;

    EXPECT_DOUBLE_EQ(mesh.data()(target_step, target_path), expected_value);
  }

  /// @brief Ensures const-correctness constraints are maintained when reading mesh data.
  TEST(NPVMeshTest, ConstDataAccessWorks)
  {
    constexpr std::size_t num_steps = 2;
    constexpr std::size_t num_paths = 2;
    constexpr std::size_t target_step = 1;
    constexpr std::size_t target_path = 1;
    constexpr double expected_value = -450.75;

    NPVMesh mesh(num_steps, num_paths);
    mesh.data()(target_step, target_path) = expected_value;

    const NPVMesh& const_mesh = mesh;
    EXPECT_DOUBLE_EQ(const_mesh.data()(target_step, target_path), expected_value);
  }

}  // namespace xva::core::test
