add_library(cdk_c STATIC
    src/ic_api.c
    src/ic_buffer.c
    src/ic_candid.c
    src/ic_principal.c
    src/ic_wasi_polyfill.c
)

target_include_directories(cdk_c PUBLIC include)

