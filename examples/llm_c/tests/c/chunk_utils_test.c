#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE (64 * 1024)

static size_t chunk_count(size_t total) {
    size_t count = 0;
    while (total > 0) {
        total = total > CHUNK_SIZE ? total - CHUNK_SIZE : 0;
        count++;
    }
    return count;
}

static size_t chunk_last_size(size_t total) {
    if (total == 0)
        return 0;
    size_t rem = total % CHUNK_SIZE;
    return rem == 0 ? CHUNK_SIZE : rem;
}

int main(void) {
    size_t totals[] = {0, CHUNK_SIZE - 1, CHUNK_SIZE, CHUNK_SIZE + 1,
                       CHUNK_SIZE * 2 + 123};
    for (size_t i = 0; i < sizeof(totals) / sizeof(totals[0]); i++) {
        size_t total = totals[i];
        size_t count = chunk_count(total);
        size_t last = chunk_last_size(total);
        printf("total=%zu -> chunks=%zu, last=%zu\n", total, count, last);
        if (total > 0 && last > CHUNK_SIZE) {
            fprintf(stderr, "chunk exceeded limit\n");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
