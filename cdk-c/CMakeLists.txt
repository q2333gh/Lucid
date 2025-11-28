add_library(cdk_c STATIC
    src/ic_api.c
    src/ic_buffer.c
    src/ic_candid.c
    src/ic_principal.c
    src/ic_wasi_polyfill.c
)

target_include_directories(cdk_c PUBLIC include)

# Enforce C11 standard for the library
target_compile_features(cdk_c PUBLIC c_std_11)

# Enable testing
option(BUILD_CDK_TESTS "Build CDK Criterion tests" OFF)
if(BUILD_CDK_TESTS)
    add_subdirectory(test)
endif()
