cmake_minimum_required(VERSION 3.15)
project(llm.c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(BASE_FLAGS "-O3 -Ofast -Wno-unused-result")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${BASE_FLAGS}")

find_package(OpenMP) # local native run infer speedup
if(OpenMP_C_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} -DOMP")
  set(OPENMP_LIBS OpenMP::OpenMP_C)
else()
  message(WARNING "OpenMP not found, building without it")
  set(OPENMP_LIBS "")
endif()

set(BASE_LIBS m ${OPENMP_LIBS})

# GGUF library (from antirez/gguf-tools)
add_library(gguf_tools STATIC
    third_party/gguf-tools/gguflib.c
    third_party/gguf-tools/fp16.c
)
target_include_directories(gguf_tools PUBLIC
    third_party/gguf-tools
)
target_compile_definitions(gguf_tools PRIVATE GGUF_TOOLS_BUILD)

# Core library (platform-independent)
add_library(llm_c_core STATIC
    src/core/model.c
    src/core/tokenizer.c
    src/core/sampler.c
    src/core/inference.c
    src/core/gguf_loader.c
)
target_include_directories(llm_c_core PUBLIC
    src/core
    src/interface
    third_party/gguf-tools
)
target_compile_definitions(llm_c_core PRIVATE LLM_C_CORE_BUILD)
target_link_libraries(llm_c_core PRIVATE m ${OPENMP_LIBS} gguf_tools)

if(NOT BUILD_TARGET_WASI)
  # Native platform library
  add_library(llm_c_platform_native STATIC
      src/platform/native/storage_impl.c
      src/platform/native/platform_impl.c
  )
  target_include_directories(llm_c_platform_native PUBLIC
      src/interface
      src/platform/native
  )
  target_link_libraries(llm_c_platform_native PUBLIC llm_c_core)

  # Native executable (replaces run_gpt2.out)
  add_executable(run_gpt2.out src/platform/native/native_main.c)
  target_include_directories(run_gpt2.out PRIVATE
      src/core
      src/platform/native
  )
  target_link_libraries(run_gpt2.out PRIVATE 
      llm_c_core 
      llm_c_platform_native
      ${BASE_LIBS}
  )

  # Legacy train_gpt2.out (keep for compatibility)
  add_executable(train_gpt2.out src/model/train_gpt2.c)
  target_link_libraries(train_gpt2.out PRIVATE ${BASE_LIBS})

  if(DEFINED ENV{WASI_SDK_PATH})
    set(WASI_CC "$ENV{WASI_SDK_PATH}/bin/clang")
    set(WASM_OUT "${CMAKE_BINARY_DIR}/run_gpt2.wasm")
    add_custom_command(
      OUTPUT ${WASM_OUT}
      COMMAND ${WASI_CC}
              --target=wasm32-wasi
              -O3 -Ofast -Wno-unused-result
              -I${CMAKE_CURRENT_SOURCE_DIR}/src/model
              ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/run_gpt2.c ${CMAKE_CURRENT_SOURCE_DIR}/src/model/tokenizer.c
              -lm -o ${WASM_OUT}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/run_gpt2.c ${CMAKE_CURRENT_SOURCE_DIR}/src/model/tokenizer.c ${CMAKE_CURRENT_SOURCE_DIR}/src/model/tokenizer.h
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Building run_gpt2.wasm via wasi-sdk"
    )
    add_custom_target(run_gpt2.wasm ALL DEPENDS ${WASM_OUT})
  endif()
endif()

if(BUILD_TARGET_WASI)
  set(CDK_BUDDY_ARENA_SIZE 2048741824 CACHE STRING
      "Buddy allocator arena size for WASM builds" FORCE)
  
  # IC platform library
  add_library(llm_c_platform_ic STATIC
      src/platform/ic/storage_impl.c
      src/platform/ic/asset_manager.c
      src/platform/ic/platform_impl.c
  )
  target_include_directories(llm_c_platform_ic PUBLIC
      src/interface
      src/platform/ic
      ${IC_CDK_C_INCLUDE_DIRS}
  )
  target_link_libraries(llm_c_platform_ic PUBLIC llm_c_core cdk_c)
  
  # IC Canister (replaces llm_c.c)
  add_executable(llm_c src/platform/ic/canister_main.c)
  target_include_directories(llm_c PRIVATE
      src/core
      src/platform/ic
      src/canister
      src/model
  )
  setup_ic_canister_target(llm_c)
  target_compile_definitions(llm_c PRIVATE LLM_C_CANISTER_BUILD)
  target_link_libraries(llm_c PRIVATE
      llm_c_core
      llm_c_platform_ic
      m
  )
  
  # Legacy canister build (keep for compatibility)
  add_executable(llm_c_legacy src/canister/llm_c.c src/canister/canister_helpers.c)
  target_include_directories(llm_c_legacy PRIVATE src/canister src/model)
  setup_ic_canister_target(llm_c_legacy)
  target_link_libraries(llm_c_legacy PRIVATE m)
endif()

if(NOT BUILD_TARGET_WASI)
  add_executable(llm_c_asset_tests 
      tests/c/test_assets.c 
      src/canister/canister_helpers.c)
  target_compile_definitions(llm_c_asset_tests PRIVATE ASSETS_TESTING)
  target_include_directories(
      llm_c_asset_tests PRIVATE
                          ${CMAKE_SOURCE_DIR}
                          ${CMAKE_SOURCE_DIR}/src/canister
                          ${CMAKE_SOURCE_DIR}/src/model
                          ${CMAKE_SOURCE_DIR}/cdk-c/include
                          ${CMAKE_SOURCE_DIR}/c_candid/runtime/runtime/include
                          ${CMAKE_SOURCE_DIR}/cdk_alloc/include)
  target_link_libraries(llm_c_asset_tests PRIVATE m)
endif()
