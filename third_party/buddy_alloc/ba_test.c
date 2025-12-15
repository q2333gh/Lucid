#include "buddy_alloc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 只在失败时多报告
void try_alloc_bytes_sysmalloc(const char *label, size_t bytes) {
    void *p = malloc(bytes);
    if (p) {
        printf("%s: ok\n size: %zu", label, bytes);
        memset(p, 0, bytes); // 测试填充
        free(p);
    } else {
        printf("%s: (系统malloc) 分配 %zu 字节失败\n", label, bytes);
    }
}

void try_alloc_bytes(const char *label, struct buddy *buddy, size_t bytes) {
    void *p = buddy_malloc(buddy, bytes);
    if (p) {
        printf("%s: ok\n size: %zu", label, bytes);
        memset(p, 0, bytes);
        buddy_free(buddy, p);
    } else {
        printf("%s: (buddy_alloc) 分配 %zu 字节失败\n", label, bytes);
    }
}

int main() {
    size_t arena_size = 16 * 1024 * 1024; // 足够大, 可以调整

    /* 外部元数据初始化方式 */
    void         *buddy_metadata = malloc(buddy_sizeof(arena_size));
    void         *buddy_arena = malloc(arena_size);
    struct buddy *buddy = buddy_init(buddy_metadata, buddy_arena, arena_size);

    // 自动测试（外部元数据）：从128 byte开始，每次加倍，直到>10MB
    size_t bytes;
    printf("==== buddy_alloc 外部元数据测试 ====\n");
    for (bytes = 128; bytes <= 10 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes("buddy_alloc", buddy, bytes);
    }

    // 直接用系统malloc比较：从128 byte开始，每次加倍，直到>10MB
    printf("==== 系统malloc测试 ====\n");
    for (bytes = 128; bytes <= 10 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes_sysmalloc("sysmalloc", bytes);
    }

    free(buddy_metadata);
    free(buddy_arena);

    /* 嵌入式元数据初始化方式 */
    buddy_arena = malloc(arena_size);
    buddy = buddy_embed(buddy_arena, arena_size);

    // 自动测试（内嵌元数据）：从128 byte开始，每次加倍，直到>10MB
    printf("==== buddy_alloc 内嵌元数据测试 ====\n");
    for (bytes = 128; bytes <= 128 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes("[内嵌] buddy_alloc", buddy, bytes);
    }

    // 系统malloc对比（再测一次）
    printf("==== 系统malloc测试(内嵌元数据) ====\n");
    for (bytes = 128; bytes <= 128 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes_sysmalloc("[内嵌] sysmalloc", bytes);
    }

    free(buddy_arena);

    return 0;
}
