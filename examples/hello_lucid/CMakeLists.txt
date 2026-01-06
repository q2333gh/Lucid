if(BUILD_TARGET_WASI)
    add_executable(hello_lucid hello_lucid.c)
    setup_ic_canister_target(hello_lucid)
    target_include_directories(
        hello_lucid PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(hello_lucid PRIVATE tinyprintf)
endif()

