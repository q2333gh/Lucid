# Define a standard option for WASI builds
option(BUILD_TARGET_WASI "Build for WASI/IC platform" OFF)

# Auto-detect if we are cross-compiling to WASI
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32" OR CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(BUILD_TARGET_WASI ON)
endif()

if (BUILD_TARGET_WASI)
    add_executable(hello_lucid hello_lucid/greet.c)
    
    # Link against the CDK library and Candid runtime
    # Assuming candid_runtime is the target name from c_candid directory
    target_link_libraries(hello_lucid PRIVATE cdk_c candid_runtime)

    # If we have the polyfill library path, import it
    if (IC_WASI_POLYFILL_PATH)
        add_library(ic_wasi_polyfill STATIC IMPORTED)
        set_target_properties(ic_wasi_polyfill PROPERTIES IMPORTED_LOCATION "${IC_WASI_POLYFILL_PATH}")
        
        # CRITICAL CHANGE: Use --whole-archive to force linking ALL polyfill functions
        # This ensures environ_get, fd_write, etc. are provided by the library and not imported
        target_link_libraries(hello_lucid PRIVATE 
            "-Wl,--whole-archive"
            ic_wasi_polyfill
            "-Wl,--no-whole-archive"
        )

        # Force referencing the raw_init symbol (optional if whole-archive works, but safe to keep)
        target_link_options(hello_lucid PRIVATE "-Wl,--undefined=raw_init")
    endif()

    # Set WASM-specific linker options for IC Canisters (Reactor model)
    target_link_options(hello_lucid PRIVATE 

        "-mexec-model=reactor"

        "-Wl,--export=__ic_wasi_polyfill_start" # C symbol for start

        # CRITICAL: Explicitly export polyfill functions to prevent LTO from stripping them
        # This is required for wasi2ic to find and replace imports
        "-Wl,--export=__ic_custom_fd_write"
        "-Wl,--export=__ic_custom_fd_read"
        "-Wl,--export=__ic_custom_fd_close"
        "-Wl,--export=__ic_custom_fd_seek"
        "-Wl,--export=__ic_custom_environ_get"
        "-Wl,--export=__ic_custom_environ_sizes_get"
        "-Wl,--export=__ic_custom_proc_exit"
        "-Wl,--export=__ic_custom_random_get"
        "-Wl,--export=__ic_custom_clock_time_get"
        "-Wl,--export=__ic_custom_clock_res_get"
        # Add others if wasi2ic complains about missing replacements

        "-Wl,--stack-first"

        "-Wl,--import-undefined"

        # Re-enable optimizations after verifying this works
        "-Wl,--gc-sections" 
        "-flto"
        "-Wl,--strip-debug" 

    )
    
    # Rename output to .wasm explicitly if needed, or rely on suffix
    set_target_properties(hello_lucid PROPERTIES SUFFIX ".wasm")

else()
    message(STATUS "Skipping examples build: Target is not WASM/WASI (BUILD_TARGET_WASI=${BUILD_TARGET_WASI})")
endif()
