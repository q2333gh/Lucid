// Test-only IC0 stable memory stub for exercising ic_storage.c logic.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ic_storage.h"

#define STABLE_TEST_MAX_PAGES 64

static uint8_t     *g_stable_memory = NULL;
static int64_t      g_stable_pages = 0;
static const size_t k_page_bytes = (size_t)IC_STABLE_PAGE_SIZE_BYTES;

void ic_storage_test_reset(void) {
    free(g_stable_memory);
    g_stable_memory = NULL;
    g_stable_pages = 0;
}

int64_t ic0_stable64_size(void) { return g_stable_pages; }

int64_t ic0_stable64_grow(int64_t new_pages) {
    if (new_pages < 0) {
        return -1;
    }

    int64_t old_pages = g_stable_pages;
    int64_t next_pages = old_pages + new_pages;
    if (next_pages > STABLE_TEST_MAX_PAGES) {
        return -1;
    }

    size_t old_bytes = (size_t)old_pages * k_page_bytes;
    size_t new_bytes = (size_t)next_pages * k_page_bytes;
    if (new_bytes == old_bytes) {
        return old_pages;
    }

    uint8_t *next = realloc(g_stable_memory, new_bytes);
    if (next == NULL && new_bytes > 0) {
        return -1;
    }

    if (new_bytes > old_bytes) {
        memset(next + old_bytes, 0, new_bytes - old_bytes);
    }

    g_stable_memory = next;
    g_stable_pages = next_pages;
    return old_pages;
}

void ic0_stable64_write(int64_t off, uint64_t src, int64_t size) {
    if (off < 0 || size < 0) {
        abort();
    }

    size_t capacity = (size_t)g_stable_pages * k_page_bytes;
    size_t end = (size_t)off + (size_t)size;
    if (end > capacity) {
        abort();
    }

    memcpy(g_stable_memory + off, (const void *)(uintptr_t)src, (size_t)size);
}

void ic0_stable64_read(uint64_t dst, int64_t off, int64_t size) {
    if (off < 0 || size < 0) {
        abort();
    }

    size_t capacity = (size_t)g_stable_pages * k_page_bytes;
    size_t end = (size_t)off + (size_t)size;
    if (end > capacity) {
        abort();
    }

    memcpy((void *)(uintptr_t)dst, g_stable_memory + off, (size_t)size);
}

_Noreturn void ic0_trap(uintptr_t src, uint32_t size) {
    (void)src;
    (void)size;
    abort();
}
