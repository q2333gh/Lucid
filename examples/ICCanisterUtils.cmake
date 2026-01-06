# Common important utilities for IC Canister targets in CMake

# Define a function to configure a target as an IC canister
function(setup_ic_canister_target target_name)
    if(NOT BUILD_TARGET_WASI)
        return()
    endif()

    # Link against the CDK library and Candid runtime
    # Assuming candid_runtime is the target name from c_candid directory
    target_link_libraries(${target_name} PRIVATE cdk_c candid_runtime)

    # If we have the polyfill library path, import it
    if (IC_WASI_POLYFILL_PATH)
        if(NOT TARGET ic_wasi_polyfill)
            add_library(ic_wasi_polyfill STATIC IMPORTED)
            set_target_properties(ic_wasi_polyfill PROPERTIES IMPORTED_LOCATION "${IC_WASI_POLYFILL_PATH}")
        endif()
        
        # CRITICAL CHANGE: Use --whole-archive to force linking ALL polyfill functions
        # This ensures environ_get, fd_write, etc. are provided by the library and not imported
        target_link_libraries(${target_name} PRIVATE 
            "-Wl,--whole-archive"
            ic_wasi_polyfill
            "-Wl,--no-whole-archive"
        )

        # Force referencing the raw_init symbol (optional if whole-archive works, but safe to keep)
        target_link_options(${target_name} PRIVATE "-Wl,--undefined=raw_init")
    endif()

    # Set WASM-specific linker options for IC Canisters (Reactor model)
    target_link_options(${target_name} PRIVATE 

        "-mexec-model=reactor"

        # Export linear "memory" for IC canister exclusive 
        "-Wl,--export-memory"

        # Export candid interface pointer function for candid-extractor
        "-Wl,--export=get_candid_pointer"

        "-Wl,--export=__ic_wasi_polyfill_start" # C symbol for start

        ####
        #  CRITICAL: Explicitly export polyfill functions to prevent LTO from stripping them
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
        ####

        # Add others if wasi2ic complains about missing replacements

        "-Wl,--stack-first"

        "-Wl,--import-undefined"

        # Re-enable optimizations after verifying this works
        "-Wl,--gc-sections" 
        "-flto"
        "-Wl,--strip-debug" 
    )
    
    # Rename output to .wasm explicitly if needed, or rely on suffix
    set_target_properties(${target_name} PROPERTIES SUFFIX ".wasm")
endfunction()

