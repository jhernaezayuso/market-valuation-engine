function(setup_target_for_coverage TARGET_NAME)
  if(NOT XVA_ENABLE_COVERAGE)
    return()
  endif()

  target_compile_definitions(${TARGET_NAME}
    PRIVATE
    XVA_ENABLE_COVERAGE_MACRO
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${TARGET_NAME}
      PRIVATE
      --coverage -O0 -g -fprofile-update=atomic
    )

    target_link_options(${TARGET_NAME}
      PRIVATE
      --coverage
    )

    add_custom_target(coverage_report
      COMMAND $<TARGET_FILE:${TARGET_NAME}>
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage_report
      COMMAND
        gcovr -r ${CMAKE_SOURCE_DIR} --filter ${CMAKE_SOURCE_DIR}/include/
        --decisions --exclude-throw-branches --html-nested --output ${CMAKE_BINARY_DIR}/coverage_report/index.html
      COMMAND
        echo "GCC coverage report generated in: ${CMAKE_BINARY_DIR}/coverage_report/index.html"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${TARGET_NAME}
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(
      ${TARGET_NAME} PRIVATE -fprofile-instr-generate -fcoverage-mapping -O0 -g -mllvm -emptyline-comment-coverage=false
    )

    target_link_options(${TARGET_NAME}
      PRIVATE
      -fprofile-instr-generate -fcoverage-mapping
    )

    add_custom_target(coverage_report
      COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE="${TARGET_NAME}.profraw" $<TARGET_FILE:${TARGET_NAME}>
      COMMAND llvm-profdata merge -sparse ${TARGET_NAME}.profraw -o ${TARGET_NAME}.profdata
      COMMAND
        llvm-cov show $<TARGET_FILE:${TARGET_NAME}>
        -instr-profile=${TARGET_NAME}.profdata -format=html
        -output-dir=${CMAKE_BINARY_DIR}/coverage_report
        -ignore-filename-regex=".*_deps.*|.*tests/.*|.*vcpkg_installed.*"
        -Xdemangler c++filt --show-line-counts-or-regions --show-branches=count
        --show-expansions --show-instantiations=true -show-directory-coverage
      COMMAND
        echo "Clang coverage report generated in: ${CMAKE_BINARY_DIR}/coverage_report/index.html"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${TARGET_NAME}
    )
  endif()
endfunction()
