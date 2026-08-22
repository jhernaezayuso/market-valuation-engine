find_package(benchmark CONFIG REQUIRED)

set(XVA_CORE_BENCHMARK_SOURCES
    xva/core/mesh_layout_bench.cpp
)

set(XVA_ENGINE_BENCHMARK_SOURCES
    xva/engine/path_generation_bench.cpp
)

set(XVA_MATH_BENCHMARK_SOURCES
    xva/math/simd_math_bench.cpp
)

set(XVA_PIPELINE_BENCHMARK_SOURCES
    xva/pipeline/cva_pipeline_bench.cpp
)

add_executable(xva_benchmarks
    ${XVA_CORE_BENCHMARK_SOURCES}
    ${XVA_ENGINE_BENCHMARK_SOURCES}
    ${XVA_MATH_BENCHMARK_SOURCES}
    ${XVA_PIPELINE_BENCHMARK_SOURCES}
)

target_link_libraries(xva_benchmarks
    PRIVATE
    xva_core
    benchmark::benchmark
    benchmark::benchmark_main
)

set(XVA_REPORT_DIR "${CMAKE_BINARY_DIR}/reports")
set(XVA_BENCHMARK_REPETITIONS 5 CACHE STRING "Repetitions used by the benchmark_report target")
set(XVA_BENCHMARK_MIN_TIME "2s" CACHE STRING "Minimum time per case used by the benchmark_report target")

add_custom_target(benchmark_report
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XVA_REPORT_DIR}
    COMMAND
      $<TARGET_FILE:xva_benchmarks>
      --benchmark_repetitions=${XVA_BENCHMARK_REPETITIONS}
      --benchmark_min_time=${XVA_BENCHMARK_MIN_TIME}
      --benchmark_report_aggregates_only=true
      --benchmark_format=json
      --benchmark_out=${XVA_REPORT_DIR}/benchmarks.json
    COMMAND ${CMAKE_COMMAND} -E echo "Benchmark report written to ${XVA_REPORT_DIR}/benchmarks.json"
    DEPENDS xva_benchmarks
    VERBATIM
)
