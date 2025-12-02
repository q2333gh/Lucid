cmake_minimum_required(VERSION 3.20)
project(ic_sdk_project C)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable testing globally
enable_testing()

# Add subdirectories
add_subdirectory(third_party/buddy_alloc) # Memory allocator
add_subdirectory(cdk_alloc)    # CDK allocator adapter
add_subdirectory(c_candid)     # C candid serde lib 
add_subdirectory(cdk-c)        # CDK main lib 
add_subdirectory(examples)     # User code examples 

# Global Optimization Flags for WASM/WASI
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
