if(BUILD_TARGET_WASI)
    add_executable(adder adder.c)
    setup_ic_canister_target(adder)
    target_include_directories(
        adder PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(adder PRIVATE tinyprintf)
endif()
