if(BUILD_TARGET_WASI)
    add_executable(simd128_example simd128_example.c)
    setup_ic_canister_target(simd128_example)
    
    # Enable SIMD128 support for wasm32 target
    target_compile_options(simd128_example PRIVATE -msimd128)
    
    # Link tinyprintf for debug output
    target_include_directories(
        simd128_example PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(simd128_example PRIVATE tinyprintf)
endif()
