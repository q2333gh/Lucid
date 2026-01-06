if(BUILD_TARGET_WASI)
    add_executable(hooks_test hooks_test.c)
    setup_ic_canister_target(hooks_test)
endif()
