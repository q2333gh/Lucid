# Define a standard option for WASI builds
option(BUILD_TARGET_WASI "Build for WASI/IC platform" OFF)

# Auto-detect if we are cross-compiling to WASI
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32" OR CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(BUILD_TARGET_WASI ON)
endif()

# Include common utilities
include(${CMAKE_CURRENT_SOURCE_DIR}/ICCanisterUtils.cmake)

# Default examples to build if not specified
if(NOT DEFINED BUILD_EXAMPLES OR "${BUILD_EXAMPLES}" STREQUAL "")
    set(BUILD_EXAMPLES "hello_lucid")
endif()

message(STATUS "Building examples: ${BUILD_EXAMPLES}")

foreach(EXAMPLE_NAME ${BUILD_EXAMPLES})
    # Check if directory exists
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}")
        # Check if CMakeLists.txt exists in the directory
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}/CMakeLists.txt")
            add_subdirectory(${EXAMPLE_NAME})
        else()
            message(WARNING "Directory ${EXAMPLE_NAME} exists but contains no CMakeLists.txt. Skipping.")
        endif()
    else()
        message(WARNING "Example directory ${EXAMPLE_NAME} not found. Skipping.")
    endif()
endforeach()
