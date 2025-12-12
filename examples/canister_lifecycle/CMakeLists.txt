if(BUILD_TARGET_WASI)
    add_executable(canister_lifecycle greet.c)
    setup_ic_canister_target(canister_lifecycle)
endif()
