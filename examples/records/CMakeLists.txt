if(BUILD_TARGET_WASI)
    add_executable(records greet.c)
    setup_ic_canister_target(records)
    target_include_directories(
        records PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(records PRIVATE tinyprintf)
endif()
