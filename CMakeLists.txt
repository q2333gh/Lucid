cmake_minimum_required(VERSION 3.20)
project(ic_sdk_project C)

# Ninja-friendly configuration
# Generate compile_commands.json for clangd and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable clang-tidy static analysis (native builds only)
# Skip for WASI cross-compilation as clang-tidy doesn't handle sysroot properly
if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,clang-analyzer-*,bugprone-*")
    endif()
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Ninja: Better dependency tracking and parallel builds
# These settings improve incremental builds with Ninja
set(CMAKE_DEPENDS_IN_PROJECT_ONLY ON)

# Enable testing globally
enable_testing()

# Add subdirectories
add_subdirectory(third_party/buddy_alloc) # Memory allocator
add_subdirectory(third_party/tinyprintf)  # Lightweight string tool
add_subdirectory(cdk_alloc)    # CDK allocator adapter
add_subdirectory(c_candid)     # C candid serde lib 
add_subdirectory(cdk-c)        # CDK main lib 
add_subdirectory(examples)     # User code examples 

# Global Optimization Flags for WASM/WASI ; STREQUAL->string equal 
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    message(STATUS "Applying WASM size optimizations...")
    add_compile_options(
        -Oz                                  # Aggressive size optimization
        -flto                                # Link Time Optimization
        -fno-exceptions                      # Disable exceptions
        -fno-unwind-tables                   # Disable unwind tables
        -fno-asynchronous-unwind-tables      # Disable async unwind tables
        -ffunction-sections                  # Place each function in its own section
        -fdata-sections                      # Place each data item in its own section
        -DNDEBUG                             # Disable assertions
    )
endif()
