if(BUILD_TARGET_WASI)
    add_executable(types_example greet.c)
    setup_ic_canister_target(types_example)
    target_include_directories(
        types_example PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(types_example PRIVATE tinyprintf)
endif()
