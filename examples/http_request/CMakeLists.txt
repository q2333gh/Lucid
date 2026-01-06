if(BUILD_TARGET_WASI)
    add_executable(http_request http_request.c)
    setup_ic_canister_target(http_request)
    target_include_directories(
        http_request PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(http_request PRIVATE tinyprintf)
endif()
