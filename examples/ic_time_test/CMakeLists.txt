if(BUILD_TARGET_WASI)
    add_executable(ic_time_test greet.c)
    setup_ic_canister_target(ic_time_test)
endif()
