find_package(GTest CONFIG REQUIRED)

set(XVA_CORE_TEST_SOURCES
    xva/core/aligned_allocator_test.cpp
    xva/core/npv_mesh_test.cpp
    xva/core/soa_buffer_test.cpp
)

add_executable(
  xva_tests
  ${XVA_CORE_TEST_SOURCES}
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
