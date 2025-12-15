#include "buddy_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main() {
    size_t arena_size = 1024; // 使用与原代码相同的arena大小

    /* 外部元数据初始化方式 */
    void *buddy_metadata = malloc(buddy_sizeof(arena_size));
    void *buddy_arena = malloc(arena_size);
    struct buddy *buddy = buddy_init(buddy_metadata, buddy_arena, arena_size);

    // 分配 128 字节内存
    void *p1 = buddy_malloc(buddy, 128);
    if (p1) {
        printf("分配了 128 字节: %p\n", p1);
        printf("buddy_metadata: %p\n", buddy_metadata);
        printf("buddy_arena: %p\n", buddy_arena);
        printf("buddy: %p\n", buddy);
        printf("p1: %p\n", p1);
        memset(p1, 0, 128);
    } else {
        printf("分配失败\n");
        printf("buddy_metadata: %p\n", buddy_metadata);
        printf("buddy_arena: %p\n", buddy_arena);
        printf("buddy: %p\n", buddy);
        printf("p1: %p\n", p1);
    }

    // 分配 256 字节内存
    void *p2 = buddy_malloc(buddy, 256);
    if (p2) {
        printf("分配了 256 字节: %p\n", p2);
        memset(p2, 1, 256);
    }

    // 释放内存
    buddy_free(buddy, p1);
    printf("释放了 p1\n");

    buddy_free(buddy, p2);
    printf("释放了 p2\n");

    // 可以继续分配更多内存
    void *p3 = buddy_malloc(buddy, 512);
    if (p3) {
        printf("再次分配了 512 字节: %p\n", p3);
    }

    free(buddy_metadata);
    free(buddy_arena);

    /* 嵌入式元数据初始化方式 */
    buddy_arena = malloc(arena_size);
    buddy = buddy_embed(buddy_arena, arena_size);

    // 分配 128 字节内存
    p1 = buddy_malloc(buddy, 128);
    if (p1) {
        printf("[内嵌] 分配了 128 字节: %p\n", p1);
        memset(p1, 0, 128);
    }

    // 分配 256 字节内存
    p2 = buddy_malloc(buddy, 256);
    if (p2) {
        printf("[内嵌] 分配了 256 字节: %p\n", p2);
        memset(p2, 1, 256);
    }

    // 释放内存
    buddy_free(buddy, p1);
    printf("[内嵌] 释放了 p1\n");

    buddy_free(buddy, p2);
    printf("[内嵌] 释放了 p2\n");

    // 可以继续分配更多内存
    p3 = buddy_malloc(buddy, 512);
    if (p3) {
        printf("[内嵌] 再次分配了 512 字节: %p\n", p3);
    }

    free(buddy_arena);

    return 0;
}
