add_library(cdk_c STATIC
    src/ic_api.c
    src/ic_buffer.c
    src/ic_principal.c
    src/ic_wasi_polyfill.c
    src/ic_candid.c
    src/ic_candid_registry.c
    src/ic_call.c
)

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

target_link_libraries(cdk_c PUBLIC cdk_alloc)

target_include_directories(cdk_c PUBLIC include)

# Enforce C11 standard for the library
target_compile_features(cdk_c PUBLIC c_std_11)

# Enable testing
option(BUILD_CDK_TESTS "Build CDK Criterion tests" OFF)
if(BUILD_CDK_TESTS)
    add_subdirectory(test)
endif()
