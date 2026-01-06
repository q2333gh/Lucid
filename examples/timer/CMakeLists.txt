if(BUILD_TARGET_WASI)
    add_executable(timer timer.c)
    setup_ic_canister_target(timer)
    target_include_directories(
        timer PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(timer PRIVATE tinyprintf)
endif()
