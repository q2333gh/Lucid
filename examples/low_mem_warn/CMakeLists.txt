if(BUILD_TARGET_WASI)
    add_executable(low_mem_warn low_mem_warn.c tinyprintf.c)
    setup_ic_canister_target(low_mem_warn)
endif()
