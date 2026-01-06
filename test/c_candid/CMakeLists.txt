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
    ${CANDID_ROOT}/build/lib
    ${CMAKE_SOURCE_DIR}/../../build-wasi/lib
)

# Test 1: Simple test (manual build)
add_executable(test_http_candid_simple.out test_http_candid_simple.c)
target_link_libraries(test_http_candid_simple.out 
    candid_runtime
)

# Test 2: Full manual test
add_executable(test_http_candid.out test_http_candid.c)
target_link_libraries(test_http_candid.out 
    candid_runtime
)

# Test 3: Simple structure test
add_executable(test_simple.out test_simple.c)
target_link_libraries(test_simple.out candid_runtime)

# Test 4: Type inference test
add_executable(test_type_inference.out test_type_inference.c)
target_link_libraries(test_type_inference.out candid_runtime)

# Test 5: Debug print test (with debug enabled)
add_executable(test_debug_print.out test_debug_print.c)
target_link_libraries(test_debug_print.out candid_runtime)
target_compile_definitions(test_debug_print.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)

# Test 6: Complete http test with cdk-c type builder
add_executable(test_http_complete.out 
    test_http_complete.c
    ${CDK_ROOT}/src/ic_http_request_type.c
)
target_link_libraries(test_http_complete.out candid_runtime)
target_compile_definitions(test_http_complete.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)
target_include_directories(test_http_complete.out PRIVATE ${CDK_ROOT}/src)

# Test 6: Full http_request_args test with debug
add_executable(test_http_args_full.out test_http_args_full.c)
target_link_libraries(test_http_args_full.out candid_runtime)
target_compile_definitions(test_http_args_full.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)

# Test 7: Simple record test (baseline)
add_executable(test_simple_record.out test_simple_record.c)
target_link_libraries(test_simple_record.out candid_runtime)
target_compile_definitions(test_simple_record.out PRIVATE CANDID_DEBUG_PRINT_ENABLED)

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
    DEPENDS test_simple.out test_type_inference.out test_debug_print.out
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all c_candid tests"
)

message(STATUS "Candid tests configured")
message(STATUS "  CANDID_ROOT: ${CANDID_ROOT}")
message(STATUS "  CDK_ROOT: ${CDK_ROOT}")
message(STATUS "Build with: cmake . && make")
message(STATUS "Run tests with: make run_tests")
