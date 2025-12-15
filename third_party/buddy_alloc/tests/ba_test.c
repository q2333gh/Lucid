#include "../buddy_alloc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Only report extra details on failure
void try_alloc_bytes_sysmalloc(const char *label, size_t bytes) {
    void *p = malloc(bytes);
    if (p) {
        printf("%s: ok\n size: %zu", label, bytes);
        memset(p, 0, bytes); // Test fill
        free(p);
    } else {
        printf("%s: (system malloc) failed to allocate %zu bytes\n", label, bytes);
    }
}

void try_alloc_bytes(const char *label, struct buddy *buddy, size_t bytes) {
    void *p = buddy_malloc(buddy, bytes);
    if (p) {
        printf("%s: ok\n size: %zu", label, bytes);
        memset(p, 0, bytes); // Test fill
        buddy_free(buddy, p);
    } else {
        printf("%s: (buddy_alloc) failed to allocate %zu bytes\n", label, bytes);
    }
}

int main() {
    size_t arena_size = 16 * 1024 * 1024; // Large enough, can be changed

    /* External metadata initialization */
    void         *buddy_metadata = malloc(buddy_sizeof(arena_size));
    void         *buddy_arena = malloc(arena_size);
    struct buddy *buddy = buddy_init(buddy_metadata, buddy_arena, arena_size);

    // Auto test (external metadata): start from 128 bytes, double each time, stop at >10MB
    size_t bytes;
    printf("==== buddy_alloc external metadata test ====\n");
    for (bytes = 128; bytes <= 10 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes("buddy_alloc", buddy, bytes);
    }

    // Compare directly with system malloc: start from 128 bytes, double each time, stop at >10MB
    printf("==== system malloc test ====\n");
    for (bytes = 128; bytes <= 10 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes_sysmalloc("sysmalloc", bytes);
    }

    free(buddy_metadata);
    free(buddy_arena);

    /* Embedded metadata initialization */
    buddy_arena = malloc(arena_size);
    buddy = buddy_embed(buddy_arena, arena_size);

    // Auto test (embedded metadata): start from 128 bytes, double each time, stop at >128MB
    printf("==== buddy_alloc embedded metadata test ====\n");
    for (bytes = 128; bytes <= 128 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes("[embedded] buddy_alloc", buddy, bytes);
    }

    // System malloc comparison (test again)
    printf("==== system malloc test (embedded metadata) ====\n");
    for (bytes = 128; bytes <= 128 * 1024 * 1024; bytes *= 2) {
        try_alloc_bytes_sysmalloc("[embedded] sysmalloc", bytes);
    }

    free(buddy_arena);

    return 0;
}
