if(BUILD_TARGET_WASI)
    add_executable(low_mem_warn low_mem_warn.c)
    setup_ic_canister_target(low_mem_warn)
    target_include_directories(
        low_mem_warn PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(low_mem_warn PRIVATE tinyprintf)
endif()
