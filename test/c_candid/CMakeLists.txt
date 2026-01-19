cmake_minimum_required(VERSION 3.16)
project(candid_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set build output to build/ directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)

# Find candid runtime library
set(CANDID_ROOT "${CMAKE_SOURCE_DIR}/../../c_candid")
set(CDK_ROOT "${CMAKE_SOURCE_DIR}/../../cdk-c")

# Include directories
include_directories(
    ${CANDID_ROOT}/runtime/runtime/include
    ${CDK_ROOT}/include
)

# Link directories (use c_candid/build/lib now)
link_directories(
    ${CMAKE_SOURCE_DIR}/../../build/lib
    ${CMAKE_SOURCE_DIR}/../../build-wasi/lib
)

set(CANDID_TEST_LIBS candid_runtime cdk_alloc tinyprintf)

# Test 1: Simple test (manual build)
add_executable(test_http_candid_simple.out test_http_candid_simple.c)
target_link_libraries(test_http_candid_simple.out 
    ${CANDID_TEST_LIBS}
)

# Test 2: Full manual test
add_executable(test_http_candid.out test_http_candid.c)
target_link_libraries(test_http_candid.out 
    ${CANDID_TEST_LIBS}
)

# Test 3: Simple structure test
add_executable(test_simple.out test_simple.c)
target_link_libraries(test_simple.out ${CANDID_TEST_LIBS})

# Test 4: Type inference test
add_executable(test_type_inference.out test_type_inference.c)
target_link_libraries(test_type_inference.out ${CANDID_TEST_LIBS})

# Test 5: Debug print test (with debug enabled)
add_executable(test_debug_print.out test_debug_print.c)
target_link_libraries(test_debug_print.out ${CANDID_TEST_LIBS})
target_compile_definitions(test_debug_print.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)

# Test 6: Complete http test with cdk-c type builder
add_executable(test_http_complete.out 
    test_http_complete.c
    ${CDK_ROOT}/src/http/ic_http_request_type.c
)
target_link_libraries(test_http_complete.out ${CANDID_TEST_LIBS})
target_compile_definitions(test_http_complete.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)
target_include_directories(test_http_complete.out PRIVATE ${CDK_ROOT}/src)

# Test 6: Full http_request_args test with debug
add_executable(test_http_args_full.out test_http_args_full.c)
target_link_libraries(test_http_args_full.out ${CANDID_TEST_LIBS})
target_compile_definitions(test_http_args_full.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)

# Test 7: Simple record test (baseline)
add_executable(test_simple_record.out test_simple_record.c)
target_link_libraries(test_simple_record.out ${CANDID_TEST_LIBS})
target_compile_definitions(test_simple_record.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)

# Test 8: Complex round-trip test (nested types)
add_executable(test_complex_roundtrip.out test_complex_roundtrip.c)
target_link_libraries(test_complex_roundtrip.out ${CANDID_TEST_LIBS})

# Test 9: Prim subset decode tests (from t_doc/candid/test/prim.test.did)
add_executable(test_prim_subset.out test_prim_subset.c)
target_link_libraries(test_prim_subset.out ${CANDID_TEST_LIBS})

# Test 10: Reference subset decode tests (from t_doc/candid/test/reference.test.did)
add_executable(test_reference_subset.out test_reference_subset.c)
target_link_libraries(test_reference_subset.out ${CANDID_TEST_LIBS})

# Test 11: Reference service/func subset decode tests
add_executable(test_reference_service_func_subset.out
    test_reference_service_func_subset.c)
target_link_libraries(test_reference_service_func_subset.out
    ${CANDID_TEST_LIBS})

# Test 12: Record subset decode tests
add_executable(test_record_subset.out test_record_subset.c)
target_link_libraries(test_record_subset.out ${CANDID_TEST_LIBS})

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND echo "=== Running test_simple.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_simple.out
    COMMAND echo ""
    COMMAND echo "=== Running test_type_inference.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_type_inference.out
    COMMAND echo ""
    COMMAND echo "=== Running test_debug_print.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_debug_print.out
    COMMAND echo ""
    COMMAND echo "=== Running test_complex_roundtrip.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_complex_roundtrip.out
    COMMAND echo ""
    COMMAND echo "=== Running test_prim_subset.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_prim_subset.out
    COMMAND echo ""
    COMMAND echo "=== Running test_reference_subset.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_reference_subset.out
    COMMAND echo ""
    COMMAND echo "=== Running test_reference_service_func_subset.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_reference_service_func_subset.out
    COMMAND echo ""
    COMMAND echo "=== Running test_record_subset.out ==="
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_record_subset.out
    DEPENDS test_simple.out test_type_inference.out test_debug_print.out
            test_complex_roundtrip.out test_prim_subset.out
            test_reference_subset.out
            test_reference_service_func_subset.out
            test_record_subset.out
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all c_candid tests"
)

message(STATUS "Candid tests configured")
message(STATUS "  CANDID_ROOT: ${CANDID_ROOT}")
message(STATUS "  CDK_ROOT: ${CDK_ROOT}")
message(STATUS "Build with: cmake . && make")
message(STATUS "Run tests with: make run_tests")
