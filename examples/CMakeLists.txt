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
        target_link_libraries(hello_lucid PRIVATE ic_wasi_polyfill)
        
        # Force referencing the raw_init symbol to ensure polyfill is linked
        target_link_options(hello_lucid PRIVATE "-Wl,--undefined=raw_init")
    endif()

    # Set WASM-specific linker options for IC Canisters (Reactor model)
    target_link_options(hello_lucid PRIVATE 
        "-mexec-model=reactor"       # IC canisters are reactors (init + update calls)
        "-Wl,--export-dynamic"       # Export symbols for the runtime
        "-Wl,--stack-first"          # Move stack to start of memory (good practice)
        "-Wl,--import-undefined"     # Allow importing symbols from host (IC system calls)
        "-Wl,--gc-sections"          # Garbage collect unused sections
        "-flto"                      # Enable LTO during linking
        "-Wl,--strip-debug"          # Strip debug information
    )
    
    # Rename output to .wasm explicitly if needed, or rely on suffix
    set_target_properties(hello_lucid PROPERTIES SUFFIX ".wasm")

else()
    message(STATUS "Skipping examples build: Target is not WASM/WASI (BUILD_TARGET_WASI=${BUILD_TARGET_WASI})")
endif()
