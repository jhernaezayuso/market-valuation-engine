cmake_minimum_required(VERSION 4.3.2)
project(
  XVAEngine
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(XVA_BUILD_TESTS "Build the test suite" OFF)
option(XVA_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

set(XVA_SIMD_WIDTH "4" CACHE STRING "Double precision SIMD lanes the whole pipeline is compiled for")
set(XVA_ARCH "native" CACHE STRING "Instruction set to target: native, a named level such as x86-64-v3, or off")
set(XVA_FP_CONTRACT "default" CACHE STRING "Floating point contraction: default, fast or off")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(coverage)

add_library(xva_core INTERFACE)

target_compile_features(xva_core INTERFACE cxx_std_26)

target_include_directories(xva_core
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_options(xva_core
  INTERFACE
  $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Werror -Wconversion>
  $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Werror -Wconversion>
)

target_compile_definitions(xva_core INTERFACE XVA_SIMD_WIDTH=${XVA_SIMD_WIDTH})

if(NOT XVA_ARCH STREQUAL "off")
  target_compile_options(xva_core INTERFACE -march=${XVA_ARCH})
  target_compile_options(xva_core INTERFACE $<$<CXX_COMPILER_ID:Clang>:-mno-avx512f>)
endif()

if(NOT XVA_FP_CONTRACT STREQUAL "default")
  target_compile_options(xva_core INTERFACE -ffp-contract=${XVA_FP_CONTRACT})
endif()

if(XVA_FP_CONTRACT STREQUAL "off")
  target_compile_definitions(xva_core INTERFACE XVA_FP_CONTRACT_OFF)
endif()

find_package(mdspan CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

target_link_libraries(xva_core INTERFACE std::mdspan TBB::tbb)

add_subdirectory(apps)

if(XVA_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
elseif(NOT XVA_BUILD_TESTS AND XVA_ENABLE_COVERAGE)
  message(
    WARNING
      "[XVA] Code coverage is enabled, but tests are disabled.\n"
      "The 'coverage_report' target will NOT be generated because it requires the test suite.\n"
      "To fix this, set XVA_BUILD_TESTS=ON.")
endif()
