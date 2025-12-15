if(BUILD_TARGET_WASI)
    # 自动找到 tinyprintf.c
    file(GLOB TINYPRINTF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/tinyprintf.c")

    add_executable(low_mem_warn greet.c ${TINYPRINTF_SRC})
    target_include_directories(low_mem_warn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    setup_ic_canister_target(low_mem_warn)
endif()
