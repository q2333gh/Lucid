if(BUILD_TARGET_WASI)
    add_executable(my_canister greet.c)
    setup_ic_canister_target(my_canister)
endif()
