# Only build examples when targeting WASM/WASI
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32" OR CMAKE_SYSTEM_NAME STREQUAL "WASI" OR BUILD_WASI)
    add_executable(hello_lucid hello_lucid/greet.c)
    target_link_libraries(hello_lucid PRIVATE cdk_c)
else()
    message(STATUS "Skipping examples build: Target is not WASM/WASI (CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})")
endif()
