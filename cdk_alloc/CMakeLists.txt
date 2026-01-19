add_library(cdk_alloc STATIC
    src/cdk_alloc.c
)

target_include_directories(cdk_alloc PUBLIC include)
if(DEFINED CDK_BUDDY_ARENA_SIZE)
  target_compile_definitions(
      cdk_alloc PUBLIC CDK_BUDDY_ARENA_SIZE=${CDK_BUDDY_ARENA_SIZE})
endif()

# cdk_alloc depends on buddy_alloc (for WASM builds)
target_link_libraries(cdk_alloc PRIVATE buddy_alloc)
