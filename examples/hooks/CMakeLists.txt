if(BUILD_TARGET_WASI)
    add_executable(hooks greet.c)
    setup_ic_canister_target(hooks)
endif()
