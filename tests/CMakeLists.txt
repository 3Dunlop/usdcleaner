if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, skipping C++ tests")
    return()
endif()

# Copy test data to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Helper function to add a test
function(add_usdcleaner_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE
        usdcleaner_core
        GTest::gtest_main
    )
    usdcleaner_set_warnings(${test_name})

    # Set test data path
    target_compile_definitions(${test_name} PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
    )

    add_test(NAME ${test_name} COMMAND ${test_name})

    # Ensure DLLs can be found at test runtime
    set(_test_path "$<TARGET_FILE_DIR:usdcleaner_core>")
    if(USD_ROOT)
        set(_test_path "${_test_path}\;${USD_ROOT}/lib\;${USD_ROOT}/bin")
    endif()
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "PATH=${_test_path}\;$ENV{PATH}"
    )
endfunction()

# Unit tests
add_usdcleaner_test(test_spatial_hash      cpp/test_spatial_hash.cpp)
add_usdcleaner_test(test_vertex_welder     cpp/test_vertex_welder.cpp)
add_usdcleaner_test(test_degenerate        cpp/test_degenerate_removal.cpp)
add_usdcleaner_test(test_lamina            cpp/test_lamina_removal.cpp)
add_usdcleaner_test(test_material_hasher   cpp/test_material_hasher.cpp)
add_usdcleaner_test(test_metadata_stripper cpp/test_metadata_stripper.cpp)
add_usdcleaner_test(test_identity_xform    cpp/test_identity_xform.cpp)
add_usdcleaner_test(test_instancing        cpp/test_instancing.cpp)
add_usdcleaner_test(test_hierarchy         cpp/test_hierarchy.cpp)
add_usdcleaner_test(test_pipeline          cpp/test_pipeline_integration.cpp)
