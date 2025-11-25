cmake_minimum_required(VERSION 3.20)

project(ic_cdk_criterion_tests LANGUAGES C VERSION 0.1.0)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CDKC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(IC_CDK_CORE_SOURCES
    "${CDKC_ROOT}/src/ic_buffer.c"
    "${CDKC_ROOT}/src/ic_principal.c"
    "${CDKC_ROOT}/src/ic_candid.c"
)

add_library(ic_cdk_core STATIC ${IC_CDK_CORE_SOURCES})
target_include_directories(ic_cdk_core PUBLIC "${CDKC_ROOT}/include")
target_compile_features(ic_cdk_core PUBLIC c_std_11)
target_compile_options(ic_cdk_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CRITERION REQUIRED IMPORTED_TARGET criterion)

add_executable(ic_cdk_tests
    test_ic_buffer.c
    test_ic_principal.c
    test_ic_candid.c
)
target_link_libraries(ic_cdk_tests PRIVATE ic_cdk_core PkgConfig::CRITERION)
target_compile_options(ic_cdk_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_test(NAME ic_cdk_tests COMMAND ic_cdk_tests)

