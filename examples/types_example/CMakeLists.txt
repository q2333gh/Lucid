if(BUILD_TARGET_WASI)
    add_executable(types_example greet.c)
    setup_ic_canister_target(types_example)
endif()
