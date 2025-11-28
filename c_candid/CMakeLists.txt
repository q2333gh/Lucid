cmake_minimum_required(VERSION 3.16)
project(c_candid_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(C_CANDID_BUILD_TESTS "Build C candid runtime tests" ON)

set(RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/runtime/runtime)
set(STUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/runtime/src)
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

file(GLOB_RECURSE CANDID_RUNTIME_SOURCES "${RUNTIME_DIR}/src/*.c")

add_library(candid_runtime STATIC
    ${CANDID_RUNTIME_SOURCES}
)
target_include_directories(candid_runtime PUBLIC ${RUNTIME_DIR}/include)

# Examples
option(C_CANDID_BUILD_EXAMPLES "Build C candid examples" ON)

# Skip executables if building for WASI reactor (Canister)
# We only need the library in that case
if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    if(C_CANDID_BUILD_EXAMPLES)
        add_executable(basic_usage ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_usage.c)
        target_link_libraries(basic_usage candid_runtime)

        add_executable(candid_style_usage ${CMAKE_CURRENT_SOURCE_DIR}/examples/candid_style_usage.c)
        target_link_libraries(candid_style_usage candid_runtime)
    endif()
    
    add_executable(c_candid_encode ${STUB_DIR}/c_candid_encode.c)
    target_include_directories(c_candid_encode PRIVATE ${RUNTIME_DIR}/include)
    target_link_libraries(c_candid_encode candid_runtime)

    add_executable(c_candid_decode ${STUB_DIR}/c_candid_decode.c)
    target_include_directories(c_candid_decode PRIVATE ${RUNTIME_DIR}/include)
    target_link_libraries(c_candid_decode candid_runtime)

    if(C_CANDID_BUILD_TESTS)
        enable_testing()
        add_executable(runtime_tests ${TESTS_DIR}/runtime_tests.c)
        target_include_directories(runtime_tests PRIVATE ${RUNTIME_DIR}/include)
        target_link_libraries(runtime_tests candid_runtime)
        add_test(NAME runtime_tests COMMAND runtime_tests)
    endif()
endif()


