if(BUILD_TARGET_WASI)
    add_executable(inter-canister-call greet.c)
    setup_ic_canister_target(inter-canister-call)
    target_include_directories(
        inter-canister-call PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(inter-canister-call PRIVATE tinyprintf)
endif()

