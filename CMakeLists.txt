cmake_minimum_required(VERSION 4.3.2)
project(
  XVAEngine
  VERSION 0.1.0
  LANGUAGES CXX
)

add_library(xva_core INTERFACE)

target_compile_features(xva_core INTERFACE cxx_std_26)
