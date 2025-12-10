if(BUILD_TARGET_WASI)
    add_executable(inter-canister-call greet.c)
    setup_ic_canister_target(inter-canister-call)
endif()

