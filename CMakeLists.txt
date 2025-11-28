cmake_minimum_required(VERSION 3.20)
project(ic_sdk_project C)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable testing globally
enable_testing()

# Add subdirectories
add_subdirectory(cdk-c)        # CDK main lib 
add_subdirectory(c_candid)     # C candid serde lib 
add_subdirectory(examples)     # User code examples 

