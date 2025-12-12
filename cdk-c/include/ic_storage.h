// Stable storage API for IC canisters
// Provides functions to save and restore data from stable memory
// Similar to ic_cdk::storage::stable_save() and stable_restore() in cdk-rs
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ic_types.h"

// WASM page size in bytes (64KB)
#define IC_STABLE_PAGE_SIZE_BYTES (64 * 1024)

// Error codes for stable storage operations
typedef enum {
    IC_STORAGE_OK = 0,
    IC_STORAGE_ERR_OUT_OF_MEMORY = 1,
    IC_STORAGE_ERR_OUT_OF_BOUNDS = 2,
    IC_STORAGE_ERR_INVALID_ARG = 3,
} ic_storage_result_t;

// Opaque structure for stable writer
struct ic_stable_writer;
typedef struct ic_stable_writer ic_stable_writer_t;

// Opaque structure for stable reader
struct ic_stable_reader;
typedef struct ic_stable_reader ic_stable_reader_t;

// =============================================================================
// Stable Memory Management (Low-level)
// =============================================================================

// Get current size of stable memory (in WASM pages)
// Returns the number of pages currently allocated
int64_t ic_stable_size(void);

// Grow stable memory by adding new pages
// new_pages: number of pages to add
// Returns: previous page count on success, -1 on failure (out of memory)
int64_t ic_stable_grow(int64_t new_pages);

// Write data to stable memory at specified offset
// offset: byte offset in stable memory
// src: source data buffer
// size: number of bytes to write
// Note: Will trap if offset + size exceeds allocated stable memory
void ic_stable_write(int64_t offset, const uint8_t *src, int64_t size);

// Read data from stable memory at specified offset
// dst: destination buffer (must be pre-allocated)
// offset: byte offset in stable memory
// size: number of bytes to read
// Note: Will trap if offset + size exceeds allocated stable memory
void ic_stable_read(uint8_t *dst, int64_t offset, int64_t size);

// =============================================================================
// Stable Writer (High-level, similar to StableWriter in cdk-rs)
// =============================================================================

// Create a new stable writer starting at offset 0
// The writer will automatically grow stable memory as needed
ic_stable_writer_t *ic_stable_writer_create(void);

// Create a new stable writer starting at specified offset
// offset: starting byte offset in stable memory
ic_stable_writer_t *ic_stable_writer_create_at(int64_t offset);

// Write data to stable memory through the writer
// writer: stable writer instance
// data: data buffer to write
// len: number of bytes to write
// Returns: IC_STORAGE_OK on success, error code on failure
ic_storage_result_t ic_stable_writer_write(ic_stable_writer_t *writer,
                                           const uint8_t      *data,
                                           size_t              len);

// Get current write offset
int64_t ic_stable_writer_offset(const ic_stable_writer_t *writer);

// Free the stable writer (does not affect stable memory)
void ic_stable_writer_free(ic_stable_writer_t *writer);

// =============================================================================
// Stable Reader (High-level, similar to StableReader in cdk-rs)
// =============================================================================

// Create a new stable reader starting at offset 0
// Reads up to the current stable memory size
ic_stable_reader_t *ic_stable_reader_create(void);

// Create a new stable reader starting at specified offset
// offset: starting byte offset in stable memory
ic_stable_reader_t *ic_stable_reader_create_at(int64_t offset);

// Read data from stable memory through the reader
// reader: stable reader instance
// data: destination buffer (must be pre-allocated)
// len: maximum number of bytes to read
// Returns: number of bytes actually read, or -1 on error
int64_t
ic_stable_reader_read(ic_stable_reader_t *reader, uint8_t *data, size_t len);

// Get current read offset
int64_t ic_stable_reader_offset(const ic_stable_reader_t *reader);

// Free the stable reader (does not affect stable memory)
void ic_stable_reader_free(ic_stable_reader_t *reader);

// =============================================================================
// High-level Storage API (similar to stable_save/stable_restore in cdk-rs)
// =============================================================================

// Save Candid-encoded data to stable memory
// This function overwrites any existing data in stable memory
// data: Candid-encoded data buffer (must include DIDL header)
// len: length of data buffer in bytes
// Returns: IC_STORAGE_OK on success, error code on failure
//
// Usage example:
//   ic_buffer_t buf;
//   ic_buffer_init(&buf);
//   // ... encode your data to Candid format into buf ...
//   ic_stable_save(buf.data, buf.size);
//   ic_buffer_free(&buf);
ic_storage_result_t ic_stable_save(const uint8_t *data, size_t len);

// Restore Candid-encoded data from stable memory
// Returns a newly allocated buffer containing the data
// data: pointer to receive the data buffer (caller must free with cdk_free)
// len: pointer to receive the data length
// Returns: IC_STORAGE_OK on success, error code on failure
//
// Usage example:
//   uint8_t *data;
//   size_t len;
//   if (ic_stable_restore(&data, &len) == IC_STORAGE_OK) {
//       // ... deserialize data from Candid format ...
//       cdk_free(data);
//   }
ic_storage_result_t ic_stable_restore(uint8_t **data, size_t *len);

// Get all stable memory as a byte buffer
// Returns a newly allocated buffer containing all stable memory
// data: pointer to receive the data buffer (caller must free with cdk_free)
// len: pointer to receive the data length
// Returns: IC_STORAGE_OK on success, error code on failure
ic_storage_result_t ic_stable_bytes(uint8_t **data, size_t *len);
