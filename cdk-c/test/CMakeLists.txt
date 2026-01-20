cmake_minimum_required(VERSION 3.20)

# Only define project if run standalone
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ic_cdk_criterion_tests LANGUAGES C VERSION 0.1.0)
    # Ninja-friendly: Generate compile_commands.json
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_DEPENDS_IN_PROJECT_ONLY ON)
endif()

include(CTest)

set(CDKC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

find_package(PkgConfig REQUIRED)
pkg_check_modules(CRITERION REQUIRED IMPORTED_TARGET criterion)

# tests
add_executable(ic_cdk_tests
    test_ic_buffer.c
    test_ic_principal.c
    test_ic_candid.c
    test_shim.c
)

# Link against the main cdk_c library if available, otherwise build locally
if(TARGET cdk_c)
    target_link_libraries(ic_cdk_tests PRIVATE cdk_c PkgConfig::CRITERION)
    # cdk_c depends on candid_runtime, and since it's a static library,
    # we might need to propagate includes or link directly if CMake doesn't transitively do it
    if(TARGET candid_runtime)
         target_link_libraries(ic_cdk_tests PRIVATE candid_runtime)
         # We need to get the source dir of c_candid to include headers
         get_target_property(CANDID_RUNTIME_SOURCE_DIR candid_runtime SOURCE_DIR)
         # The SOURCE_DIR of the target is where CMakeLists.txt is.
         # We need runtime/runtime/include relative to that.
         # But we don't easily know where candid_runtime was defined if it came from parent.
         
         # Actually, cdk_c should expose the include directory if we set it to PUBLIC/INTERFACE
         # In cdk-c/CMakeLists.txt we set it PRIVATE. 
         # Let's add it here manually for the test.
         get_filename_component(C_CANDID_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../c_candid" ABSOLUTE)
         target_include_directories(ic_cdk_tests PRIVATE "${C_CANDID_ROOT}/runtime/runtime/include")
    endif()
else()
    # Standalone build fallback
    set(IC_CDK_CORE_SOURCES
        "${CDKC_ROOT}/src/ic_buffer.c"
        "${CDKC_ROOT}/src/ic_principal.c"
        "${CDKC_ROOT}/src/ic_candid.c"
    )
    add_library(ic_cdk_core STATIC ${IC_CDK_CORE_SOURCES})
    target_include_directories(ic_cdk_core PUBLIC "${CDKC_ROOT}/include")
    target_compile_features(ic_cdk_core PUBLIC c_std_11)
    target_compile_options(ic_cdk_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(ic_cdk_tests PRIVATE ic_cdk_core PkgConfig::CRITERION)
endif()

target_compile_options(ic_cdk_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME ic_cdk_tests COMMAND ic_cdk_tests)

add_executable(ic_cdk_storage_tests
    test_ic_storage.c
    test_ic_storage_stub.c
    "${CDKC_ROOT}/src/ic_storage.c"
)

target_include_directories(ic_cdk_storage_tests PRIVATE "${CDKC_ROOT}/include")
target_compile_features(ic_cdk_storage_tests PRIVATE c_std_11)
target_compile_options(ic_cdk_storage_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(ic_cdk_storage_tests PRIVATE PkgConfig::CRITERION)

if(TARGET cdk_alloc)
    target_link_libraries(ic_cdk_storage_tests PRIVATE cdk_alloc)
else()
    target_sources(ic_cdk_storage_tests PRIVATE
        "${CDKC_ROOT}/../cdk_alloc/src/cdk_alloc.c"
    )
    target_include_directories(ic_cdk_storage_tests PRIVATE
        "${CDKC_ROOT}/../cdk_alloc/include"
    )
endif()

add_test(NAME ic_cdk_storage_tests COMMAND ic_cdk_storage_tests)

add_executable(ic_cdk_shim_libc_tests
    test_shim_libc.c
    "${CDKC_ROOT}/src/shim_common.c"
    "${CDKC_ROOT}/src/shim_libc.c"
)

target_include_directories(ic_cdk_shim_libc_tests PRIVATE "${CDKC_ROOT}/include")
target_compile_features(ic_cdk_shim_libc_tests PRIVATE c_std_11)
target_compile_options(ic_cdk_shim_libc_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(ic_cdk_shim_libc_tests PRIVATE SHIM_LIBC_ENABLE)
target_link_libraries(ic_cdk_shim_libc_tests PRIVATE PkgConfig::CRITERION)

if(TARGET cdk_alloc)
    target_link_libraries(ic_cdk_shim_libc_tests PRIVATE cdk_alloc)
else()
    target_sources(ic_cdk_shim_libc_tests PRIVATE
        "${CDKC_ROOT}/../cdk_alloc/src/cdk_alloc.c"
    )
    target_include_directories(ic_cdk_shim_libc_tests PRIVATE
        "${CDKC_ROOT}/../cdk_alloc/include"
    )
endif()

add_test(NAME ic_cdk_shim_libc_tests COMMAND ic_cdk_shim_libc_tests)
