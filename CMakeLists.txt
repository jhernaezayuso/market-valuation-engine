cmake_minimum_required(VERSION 4.3.2)
project(
  XVAEngine
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(XVA_BUILD_TESTS "Build the test suite" OFF)
option(XVA_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

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
