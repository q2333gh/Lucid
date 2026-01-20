/*
 * Stable Storage Implementation
 *
 * Provides persistent storage APIs for IC canisters using stable memory.
 * Implements:
 * - Low-level stable memory operations (read, write, grow, size)
 * - Stable writer: Sequential write interface with automatic growth
 * - Stable reader: Sequential read interface with bounds checking
 * - High-level save/restore: Save entire byte arrays to stable memory
 *
 * Stable memory persists across canister upgrades, making it essential for
 * state preservation. This module provides both low-level control and
 * convenient high-level APIs for common use cases.
 */
#include "ic_storage.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ic0.h"
#include "idl/cdk_alloc.h"

// =============================================================================
// Stable Memory Management (Low-level)
// =============================================================================

int64_t ic_stable_size(void) { return ic0_stable64_size(); }

int64_t ic_stable_grow(int64_t new_pages) {
    int64_t result = ic0_stable64_grow(new_pages);
    // In IC spec, stable64_grow returns -1 (or u64::MAX in Rust) on failure
    // When u64::MAX is cast to int64_t, it becomes -1 (two's complement)
    // We return -1 to indicate failure
    if (result == -1) {
        return -1;
    }
    // Also check for negative values (shouldn't happen for valid page counts)
    if (result < 0) {
        return -1;
    }
    return result;
}

void ic_stable_write(int64_t offset, const uint8_t *src, int64_t size) {
    if (src == NULL || size < 0) {
        ic0_trap((uintptr_t) "ic_stable_write: invalid arguments", 35);
    }
    if (size == 0) {
        return;
    }
    ic0_stable64_write(offset, (uint64_t)(uintptr_t)src, size);
}

void ic_stable_read(uint8_t *dst, int64_t offset, int64_t size) {
    if (dst == NULL || size < 0) {
        ic0_trap((uintptr_t) "ic_stable_read: invalid arguments", 34);
    }
    if (size == 0) {
        return;
    }
    ic0_stable64_read((uint64_t)(uintptr_t)dst, offset, size);
}

// =============================================================================
// Stable Writer Implementation
// =============================================================================

struct ic_stable_writer {
    int64_t offset;   // Current write offset in bytes
    int64_t capacity; // Current capacity in pages
};

static int64_t calculate_required_pages(int64_t bytes) {
    // Calculate pages needed: ceil(bytes / IC_STABLE_PAGE_SIZE_BYTES)
    int64_t pages = bytes / IC_STABLE_PAGE_SIZE_BYTES;
    if (bytes % IC_STABLE_PAGE_SIZE_BYTES != 0) {
        pages++;
    }
    return pages;
}

static ic_storage_result_t stable_writer_grow(ic_stable_writer_t *writer,
                                              int64_t             new_pages) {
    int64_t old_pages = ic_stable_grow(new_pages);
    if (old_pages < 0) {
        return IC_STORAGE_ERR_OUT_OF_MEMORY;
    }
    writer->capacity = old_pages + new_pages;
    return IC_STORAGE_OK;
}

ic_stable_writer_t *ic_stable_writer_create(void) {
    return ic_stable_writer_create_at(0);
}

ic_stable_writer_t *ic_stable_writer_create_at(int64_t offset) {
    ic_stable_writer_t *writer =
        (ic_stable_writer_t *)cdk_malloc(sizeof(ic_stable_writer_t));
    if (writer == NULL) {
        return NULL;
    }

    writer->offset = offset;
    writer->capacity = ic_stable_size();

    // Ensure we have enough capacity for the starting offset
    int64_t required_capacity_bytes = offset;
    int64_t required_capacity_pages =
        calculate_required_pages(required_capacity_bytes);
    if (required_capacity_pages > writer->capacity) {
        int64_t additional_pages = required_capacity_pages - writer->capacity;
        if (stable_writer_grow(writer, additional_pages) != IC_STORAGE_OK) {
            cdk_free(writer);
            return NULL;
        }
    }

    return writer;
}

ic_storage_result_t ic_stable_writer_write(ic_stable_writer_t *writer,
                                           const uint8_t      *data,
                                           size_t              len) {
    if (writer == NULL || data == NULL) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }

    if (len == 0) {
        return IC_STORAGE_OK;
    }

    // Calculate required capacity
    int64_t required_capacity_bytes = writer->offset + (int64_t)len;
    int64_t required_capacity_pages =
        calculate_required_pages(required_capacity_bytes);
    int64_t current_pages = writer->capacity;
    int64_t additional_pages_required =
        required_capacity_pages > current_pages
            ? required_capacity_pages - current_pages
            : 0;

    // Grow if needed
    if (additional_pages_required > 0) {
        ic_storage_result_t result =
            stable_writer_grow(writer, additional_pages_required);
        if (result != IC_STORAGE_OK) {
            return result;
        }
    }

    // Write data
    ic_stable_write(writer->offset, data, (int64_t)len);
    writer->offset += (int64_t)len;

    return IC_STORAGE_OK;
}

int64_t ic_stable_writer_offset(const ic_stable_writer_t *writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->offset;
}

void ic_stable_writer_free(ic_stable_writer_t *writer) {
    if (writer != NULL) {
        cdk_free(writer);
    }
}

// =============================================================================
// Stable Reader Implementation
// =============================================================================

struct ic_stable_reader {
    int64_t offset;   // Current read offset in bytes
    int64_t capacity; // Capacity in pages (cached at creation time)
};

ic_stable_reader_t *ic_stable_reader_create(void) {
    return ic_stable_reader_create_at(0);
}

ic_stable_reader_t *ic_stable_reader_create_at(int64_t offset) {
    ic_stable_reader_t *reader =
        (ic_stable_reader_t *)cdk_malloc(sizeof(ic_stable_reader_t));
    if (reader == NULL) {
        return NULL;
    }

    reader->offset = offset;
    reader->capacity = ic_stable_size();

    return reader;
}

int64_t
ic_stable_reader_read(ic_stable_reader_t *reader, uint8_t *data, size_t len) {
    if (reader == NULL || data == NULL) {
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    // Calculate available bytes
    int64_t capacity_bytes = reader->capacity * IC_STABLE_PAGE_SIZE_BYTES;
    int64_t available_bytes = capacity_bytes - reader->offset;

    if (available_bytes <= 0) {
        return 0; // EOF
    }

    // Determine how much to read
    int64_t read_len = (int64_t)len;
    if (read_len > available_bytes) {
        read_len = available_bytes;
    }

    // Read data
    ic_stable_read(data, reader->offset, read_len);
    reader->offset += read_len;

    return read_len;
}

int64_t ic_stable_reader_offset(const ic_stable_reader_t *reader) {
    if (reader == NULL) {
        return 0;
    }
    return reader->offset;
}

void ic_stable_reader_free(ic_stable_reader_t *reader) {
    if (reader != NULL) {
        cdk_free(reader);
    }
}

// =============================================================================
// Stable IO Implementation
// =============================================================================

struct ic_stable_io {
    int64_t offset;   // Current offset in bytes
    int64_t capacity; // Capacity in pages (cached at creation time)
};

static ic_storage_result_t stable_io_grow(ic_stable_io_t *io,
                                          int64_t         new_pages) {
    if (new_pages <= 0) {
        return IC_STORAGE_OK;
    }
    int64_t old_pages = ic_stable_grow(new_pages);
    if (old_pages < 0) {
        return IC_STORAGE_ERR_OUT_OF_MEMORY;
    }
    io->capacity = old_pages + new_pages;
    return IC_STORAGE_OK;
}

ic_stable_io_t *ic_stable_io_create(void) { return ic_stable_io_create_at(0); }

ic_stable_io_t *ic_stable_io_create_at(int64_t offset) {
    if (offset < 0) {
        return NULL;
    }

    ic_stable_io_t *io = (ic_stable_io_t *)cdk_malloc(sizeof(ic_stable_io_t));
    if (io == NULL) {
        return NULL;
    }

    io->offset = offset;
    io->capacity = ic_stable_size();
    if (io->capacity < 0) {
        cdk_free(io);
        return NULL;
    }

    return io;
}

ic_storage_result_t
ic_stable_io_write(ic_stable_io_t *io, const uint8_t *data, size_t len) {
    if (io == NULL || (data == NULL && len > 0)) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return IC_STORAGE_OK;
    }
    if (io->offset < 0 || len > (size_t)INT64_MAX) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }
    if (io->offset > INT64_MAX - (int64_t)len) {
        return IC_STORAGE_ERR_OUT_OF_BOUNDS;
    }

    int64_t required_bytes = io->offset + (int64_t)len;
    int64_t required_pages = calculate_required_pages(required_bytes);

    if (required_pages > io->capacity) {
        int64_t             additional_pages = required_pages - io->capacity;
        ic_storage_result_t result = stable_io_grow(io, additional_pages);
        if (result != IC_STORAGE_OK) {
            return result;
        }
    }

    ic_stable_write(io->offset, data, (int64_t)len);
    io->offset += (int64_t)len;
    return IC_STORAGE_OK;
}

int64_t ic_stable_io_read(ic_stable_io_t *io, uint8_t *data, size_t len) {
    if (io == NULL || data == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (io->offset < 0 || len > (size_t)INT64_MAX) {
        return -1;
    }
    if (io->capacity < 0 ||
        io->capacity > INT64_MAX / IC_STABLE_PAGE_SIZE_BYTES) {
        return -1;
    }

    int64_t capacity_bytes = io->capacity * IC_STABLE_PAGE_SIZE_BYTES;
    int64_t available_bytes = capacity_bytes - io->offset;
    if (available_bytes <= 0) {
        return 0;
    }

    int64_t read_len = (int64_t)len;
    if (read_len > available_bytes) {
        read_len = available_bytes;
    }

    ic_stable_read(data, io->offset, read_len);
    io->offset += read_len;
    return read_len;
}

ic_storage_result_t ic_stable_io_seek(ic_stable_io_t         *io,
                                      int64_t                 offset,
                                      ic_stable_seek_whence_t whence) {
    if (io == NULL) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }

    int64_t base = 0;
    if (whence == IC_STABLE_SEEK_SET) {
        base = 0;
    } else if (whence == IC_STABLE_SEEK_CUR) {
        base = io->offset;
    } else if (whence == IC_STABLE_SEEK_END) {
        if (io->capacity < 0 ||
            io->capacity > INT64_MAX / IC_STABLE_PAGE_SIZE_BYTES) {
            return IC_STORAGE_ERR_OUT_OF_BOUNDS;
        }
        base = io->capacity * IC_STABLE_PAGE_SIZE_BYTES;
    } else {
        return IC_STORAGE_ERR_INVALID_ARG;
    }

    if (offset >= 0) {
        if (base > INT64_MAX - offset) {
            return IC_STORAGE_ERR_OUT_OF_BOUNDS;
        }
    } else {
        if (base < -offset) {
            return IC_STORAGE_ERR_OUT_OF_BOUNDS;
        }
    }

    io->offset = base + offset;
    return IC_STORAGE_OK;
}

int64_t ic_stable_io_offset(const ic_stable_io_t *io) {
    if (io == NULL) {
        return 0;
    }
    return io->offset;
}

void ic_stable_io_free(ic_stable_io_t *io) {
    if (io != NULL) {
        cdk_free(io);
    }
}

// =============================================================================
// High-level Storage API
// =============================================================================

ic_storage_result_t ic_stable_save(const uint8_t *data, size_t len) {
    if (data == NULL && len > 0) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }

    ic_stable_writer_t *writer = ic_stable_writer_create();
    if (writer == NULL) {
        return IC_STORAGE_ERR_OUT_OF_MEMORY;
    }

    ic_storage_result_t result = ic_stable_writer_write(writer, data, len);
    ic_stable_writer_free(writer);

    return result;
}

ic_storage_result_t ic_stable_restore(uint8_t **data, size_t *len) {
    if (data == NULL || len == NULL) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }

    return ic_stable_bytes(data, len);
}

ic_storage_result_t ic_stable_bytes(uint8_t **data, size_t *len) {
    if (data == NULL || len == NULL) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }

    int64_t pages = ic_stable_size();
    if (pages < 0) {
        return IC_STORAGE_ERR_OUT_OF_MEMORY;
    }

    int64_t total_bytes = pages * IC_STABLE_PAGE_SIZE_BYTES;
    if (total_bytes == 0) {
        *data = NULL;
        *len = 0;
        return IC_STORAGE_OK;
    }

    // Check for overflow (shouldn't happen in practice, but be safe)
    if (total_bytes < 0 || (size_t)total_bytes > SIZE_MAX) {
        return IC_STORAGE_ERR_OUT_OF_MEMORY;
    }

    uint8_t *buffer = (uint8_t *)cdk_malloc((size_t)total_bytes);
    if (buffer == NULL) {
        return IC_STORAGE_ERR_OUT_OF_MEMORY;
    }

    // Read all stable memory
    ic_stable_read(buffer, 0, total_bytes);

    *data = buffer;
    *len = (size_t)total_bytes;

    return IC_STORAGE_OK;
}
