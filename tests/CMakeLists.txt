find_package(GTest CONFIG REQUIRED)

set(XVA_AGGREGATION_TEST_SOURCES
    xva/aggregation/cva_aggregator_test.cpp
)

set(XVA_CORE_TEST_SOURCES
    xva/core/aligned_allocator_test.cpp
    xva/core/npv_mesh_test.cpp
    xva/core/soa_buffer_test.cpp
)

set(XVA_ENGINE_TEST_SOURCES
    xva/engine/monte_carlo_engine_test.cpp
)

set(XVA_INSTRUMENTS_TEST_SOURCES
    xva/instruments/interest_rate_swap_test.cpp
)

set(XVA_MATH_TEST_SOURCES
    xva/math/inverse_cdf_test.cpp
    xva/math/philox_rng_test.cpp
)

set(XVA_MODELS_TEST_SOURCES
    xva/models/hull_white_1f_test.cpp
    xva/models/yield_curve_test.cpp
)

set(XVA_PRICING_TEST_SOURCES
    xva/pricing/interest_rate_swap_pricer_test.cpp
)

add_executable(xva_tests
    ${XVA_AGGREGATION_TEST_SOURCES}
    ${XVA_CORE_TEST_SOURCES}
    ${XVA_ENGINE_TEST_SOURCES}
    ${XVA_INSTRUMENTS_TEST_SOURCES}
    ${XVA_MATH_TEST_SOURCES}
    ${XVA_MODELS_TEST_SOURCES}
    ${XVA_PRICING_TEST_SOURCES}
)

target_link_libraries(xva_tests
    PRIVATE
    xva_core
    GTest::gtest
    GTest::gtest_main
)

setup_target_for_coverage(xva_tests)

include(GoogleTest)
gtest_discover_tests(xva_tests)
