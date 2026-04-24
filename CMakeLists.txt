cmake_minimum_required(VERSION 4.3.2)
project(
  XVAEngine
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(XVA_BUILD_TESTS "Build the test suite" OFF)

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
endif()
