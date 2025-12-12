if(BUILD_TARGET_WASI)
    add_executable(timer greet.c)
    setup_ic_canister_target(timer)
endif()
