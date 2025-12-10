if(BUILD_TARGET_WASI)
    add_executable(adder greet.c)
    setup_ic_canister_target(adder)
endif()
