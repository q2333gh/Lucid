if(BUILD_TARGET_WASI)
    add_executable(records records.c)
    setup_ic_canister_target(records)
    target_include_directories(
        records PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(records PRIVATE tinyprintf)
else()
    # Native build for size analysis
    add_executable(records_native records.c main_native.c)
    target_include_directories(
        records_native PRIVATE "${CMAKE_SOURCE_DIR}/third_party/tinyprintf")
    target_link_libraries(records_native PRIVATE cdk_c candid_runtime tinyprintf)
    # Set output name
    set_target_properties(records_native PROPERTIES OUTPUT_NAME "records")
endif()
