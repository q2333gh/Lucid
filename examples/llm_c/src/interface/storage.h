#ifndef LLM_C_STORAGE_H
#define LLM_C_STORAGE_H

#include <stddef.h>
#include <stdint.h>

/**
 * Storage abstraction interface for platform-independent data access.
 *
 * This interface allows the core inference engine to read data without
 * knowing whether it's coming from native file system or IC stable memory.
 */
typedef struct llm_storage {
    /**
     * Read checkpoint model data.
     *
     * @param ctx Storage context (platform-specific)
     * @param name Asset name (e.g., "checkpoint")
     * @param data Output buffer (caller must free)
     * @param len Output length
     * @return 0 on success, -1 on error
     */
    int (*read_checkpoint)(void       *ctx,
                           const char *name,
                           uint8_t   **data,
                           size_t     *len);

    /**
     * Read tokenizer asset (vocab.json or vocab.bpe).
     *
     * @param ctx Storage context
     * @param name Asset name (e.g., "vocab", "merges")
     * @param data Output buffer (caller must free)
     * @param len Output length
     * @return 0 on success, -1 on error
     */
    int (*read_asset)(void *ctx, const char *name, uint8_t **data, size_t *len);

    /**
     * Read token sequence (e.g., stories dataset).
     *
     * @param ctx Storage context
     * @param name Asset name (e.g., "stories")
     * @param offset Token offset
     * @param count Number of tokens to read
     * @param tokens Output token array (caller must free)
     * @return 0 on success, -1 on error
     */
    int (*read_tokens)(
        void *ctx, const char *name, int offset, int count, int **tokens);

    /**
     * Read GGUF model file.
     *
     * @param ctx Storage context
     * @param name GGUF file name/path
     * @param data Output file data (caller must free for IC, file-backed for
     * native)
     * @param len Output file length
     * @return 0 on success, -1 on error
     *
     * Note: For native platform, data may point to file path string.
     * For IC platform, data contains actual file contents.
     */
    int (*read_gguf)(void *ctx, const char *name, uint8_t **data, size_t *len);

    /**
     * Platform-specific context.
     * Native: file paths structure
     * IC: asset manager with stable memory offsets
     */
    void *context;
} llm_storage_t;

#endif // LLM_C_STORAGE_H
