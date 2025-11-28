cmake_minimum_required(VERSION 3.18)
project(ic_sdk_project C)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add subdirectories
add_subdirectory(cdk-c)        # CDK main lib 
add_subdirectory(examples)     # User code examples
add_subdirectory(c_candid)     # C candid serde lib 

