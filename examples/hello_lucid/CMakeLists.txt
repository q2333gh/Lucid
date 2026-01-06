if(BUILD_TARGET_WASI)
    add_executable(hello_lucid hello_lucid.c)
    setup_ic_canister_target(hello_lucid)
endif()

