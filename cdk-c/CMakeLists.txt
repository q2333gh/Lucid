# Source files for cdk_c library
set(CDK_C_SOURCES
    src/ic_api.c
    src/ic_args.c
    src/ic_buffer.c
    src/ic_principal.c
    src/ic_wasi_polyfill.c
    src/ic_candid.c
    src/ic_candid_registry.c
    src/ic_call.c
    src/ic_storage.c
    src/ic_timer.c
    src/shim_common.c
    src/shim_libc.c
    src/http/ic_http_request.c
    src/http/ic_http_request_cost.c
    src/http/ic_http_request_build.c
    src/http/ic_http_request_parse.c
    src/http/ic_http_request_type.c
)

# Add IC0 stub for native builds (not WASI)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    list(APPEND CDK_C_SOURCES src/ic0_stub.c)
    list(APPEND CDK_C_SOURCES src/platform/native/shim_native.c)
else()
    list(APPEND CDK_C_SOURCES src/platform/ic/shim_ic.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    target_compile_definitions(cdk_c PUBLIC SHIM_LIBC_ENABLE)
endif()

add_library(cdk_c STATIC ${CDK_C_SOURCES})

# Add c_candid dependency
if(NOT TARGET candid_runtime)
    get_filename_component(C_CANDID_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../c_candid" ABSOLUTE)
    if(EXISTS "${C_CANDID_ROOT}/CMakeLists.txt")
        add_subdirectory("${C_CANDID_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/c_candid")
    else()
        message(WARNING "c_candid not found at ${C_CANDID_ROOT}")
    endif()
endif()

if(TARGET candid_runtime)
    target_link_libraries(cdk_c PUBLIC candid_runtime)
    get_target_property(CANDID_RUNTIME_SOURCE_DIR candid_runtime SOURCE_DIR)
    target_include_directories(cdk_c PUBLIC "${CANDID_RUNTIME_SOURCE_DIR}/runtime/runtime/include")
endif()

target_link_libraries(cdk_c PUBLIC cdk_alloc tinyprintf)

target_include_directories(cdk_c PUBLIC include)

# Enforce C11 standard for the library
target_compile_features(cdk_c PUBLIC c_std_11)

# Enable testing
option(BUILD_CDK_TESTS "Build CDK Criterion tests" OFF)
if(BUILD_CDK_TESTS)
    add_subdirectory(test)
endif()
