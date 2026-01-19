#include <inttypes.h>

#include "canister_helpers.h"

static char *duplicate_string(const char *src) {
    if (!src)
        return NULL;
    size_t len = strlen(src);
    char  *dst = (char *)malloc(len + 1);
    if (!dst)
        return NULL;
    memcpy(dst, src, len + 1);
    return dst;
}

#ifndef ASSETS_TESTING
#define TESTING
#include "../model/tokenizer.c"
#include "../model/train_gpt2.c"
#endif

typedef struct {
    char    name[MAX_ASSET_NAME];
    int64_t offset;
    int64_t length;
} asset_entry_t;

static asset_entry_t       g_asset_table[MAX_ASSETS];
static size_t              g_asset_count = 0;
static ic_stable_writer_t *g_writer = NULL;
static const char *const kRequiredAssets[] = {"checkpoint", "vocab", "merges"};
static const size_t      kRequiredAssetCount =
    sizeof(kRequiredAssets) / sizeof(kRequiredAssets[0]);
#ifndef ASSETS_TESTING
static GPT2           g_model;
static int            g_model_ready = 0;
static GPT2Tokenizer *g_tokenizer = NULL;
static uint8_t       *g_vocab_data = NULL;
static size_t         g_vocab_len = 0;
static uint8_t       *g_merges_data = NULL;
static size_t         g_merges_len = 0;
static float         *g_work_logits = NULL;
static float         *g_work_probs = NULL;
static size_t         g_vocab_size = 0;
#endif

#define INFER_STATE_MAGIC 0x534D4C4C
#define INFER_STATE_VERSION 1

typedef struct {
    uint32_t           magic;
    uint32_t           version;
    uint32_t           seq_len;
    uint32_t           max_len;
    unsigned long long rng_state;
    uint32_t           done;
    uint32_t           reserved;
} infer_state_header_t;

void llm_c_reset_asset_state(void) {
    g_asset_count = 0;
    memset(g_asset_table, 0, sizeof(g_asset_table));
    g_writer = NULL;
    llm_c_reset_inference_cache();
}

size_t llm_c_asset_count(void) { return g_asset_count; }

void trap_msg(const char *msg) { ic_api_trap(msg); }

static void sanitize_asset_name(char *dst, const char *src) {
    size_t idx = 0;
    while (idx < MAX_ASSET_NAME - 1 && src[idx]) {
        dst[idx] = src[idx];
        idx++;
    }
    dst[idx] = '\0';
}

#ifndef ASSETS_TESTING
void llm_c_reset_inference_cache(void) {
    if (g_model_ready) {
        gpt2_free(&g_model);
        g_model_ready = 0;
    }
    if (g_tokenizer) {
        tokenizer_free(g_tokenizer);
        g_tokenizer = NULL;
    }
    free(g_vocab_data);
    free(g_merges_data);
    g_vocab_data = NULL;
    g_vocab_len = 0;
    g_merges_data = NULL;
    g_merges_len = 0;
    free(g_work_logits);
    free(g_work_probs);
    g_work_logits = NULL;
    g_work_probs = NULL;
    g_vocab_size = 0;
}
#else
void  llm_c_reset_inference_cache(void) {}
#endif

static asset_entry_t *lookup_asset(const char *name) {
    ic_api_debug_printf("lookup_asset(%s) start; g_asset_count=%zu\n", name,
                        g_asset_count);
    for (size_t i = 0; i < g_asset_count; i++) {
        if (strncmp(g_asset_table[i].name, name, MAX_ASSET_NAME) == 0) {
            ic_api_debug_printf("lookup_asset(%s) found idx=%zu offset=%" PRId64
                                " len=%" PRId64 "\n",
                                name, i, g_asset_table[i].offset,
                                g_asset_table[i].length);
            return &g_asset_table[i];
        }
    }
    ic_api_debug_printf("lookup_asset(%s) end (not found)\n", name);
    return NULL;
}

#ifndef ASSETS_TESTING
int llm_c_asset_exists(const char *name) {
    if (!name)
        return 0;
    asset_entry_t *entry = lookup_asset(name);
    if (!entry)
        return 0;
    return entry->length > 0 ? 1 : 0;
}
#endif

#ifndef ASSETS_TESTING
int read_asset_tokens_u16(const char *name,
                          int         offset,
                          int         count,
                          int       **out_tokens) {
    if (!name || !out_tokens || count <= 0 || offset < 0)
        trap_msg("read_asset_tokens_u16: invalid arguments");
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0)
        trap_msg("read_asset_tokens_u16: asset missing");
    int64_t byte_offset = entry->offset + (int64_t)offset * sizeof(uint16_t);
    int64_t byte_len = (int64_t)count * sizeof(uint16_t);
    if (byte_offset < entry->offset ||
        byte_offset + byte_len > entry->offset + entry->length)
        trap_msg("read_asset_tokens_u16: out of bounds");
    uint16_t *buffer = (uint16_t *)malloc((size_t)count * sizeof(uint16_t));
    if (!buffer)
        trap_msg("read_asset_tokens_u16: buffer allocation failed");
    ic_stable_read((uint8_t *)buffer, byte_offset, byte_len);
    int *tokens = (int *)malloc((size_t)count * sizeof(int));
    if (!tokens) {
        free(buffer);
        trap_msg("read_asset_tokens_u16: token allocation failed");
    }
    for (int i = 0; i < count; i++) {
        tokens[i] = (int)buffer[i];
    }
    free(buffer);
    *out_tokens = tokens;
    return count;
}
#endif

static void
get_asset_range(const char *name, int64_t *out_offset, int64_t *out_length) {
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0)
        trap_msg("requested asset is missing");
    if (out_offset)
        *out_offset = entry->offset;
    if (out_length)
        *out_length = entry->length;
}

static void ensure_writer(void) {
    if (g_writer)
        return;
    g_writer = ic_stable_writer_create();
    if (!g_writer)
        trap_msg("stable writer initialization failed");
}

void write_asset_data(const char *name, const uint8_t *data, size_t len) {
    ic_api_debug_printf("write_asset_data(%s) start len=%zu\n", name, len);
    if (!name || !data || len == 0)
        trap_msg("load_asset: invalid arguments");
    if (len > INT64_MAX)
        trap_msg("load_asset: asset too large");
    ensure_writer();
    asset_entry_t *entry = lookup_asset(name);
    if (!entry) {
        if (g_asset_count >= MAX_ASSETS)
            trap_msg("load_asset: asset table full");
        entry = &g_asset_table[g_asset_count++];
        memset(entry, 0, sizeof(*entry));
        sanitize_asset_name(entry->name, name);
        entry->offset = ic_stable_writer_offset(g_writer);
        entry->length = 0;
    }
    int64_t start = ic_stable_writer_offset(g_writer);
    if (start != entry->offset + entry->length)
        trap_msg("load_asset: non-contiguous append");
    ic_storage_result_t result =
        ic_stable_writer_write(g_writer, data, (int64_t)len);
    if (result != IC_STORAGE_OK)
        trap_msg("load_asset: stable memory write failed");
    entry->length += (int64_t)len;
    ic_api_debug_printf("write_asset_data(%s) stored offset=%" PRId64
                        " len=%" PRId64 "\n",
                        name, entry->offset, entry->length);
}

void read_asset_blob(const char *name, uint8_t **out_data, size_t *out_len) {
    ic_api_debug_printf("read_asset_blob(%s) start\n", name);
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0)
        trap_msg("requested asset is missing");
    if ((uint64_t)entry->length > SIZE_MAX)
        trap_msg("requested asset is too large");
    size_t   len = (size_t)entry->length;
    uint8_t *buffer = (uint8_t *)malloc(len);
    if (!buffer)
        trap_msg("failed to allocate asset buffer");
    ic_stable_read(buffer, entry->offset, entry->length);
    *out_data = buffer;
    *out_len = len;
    ic_api_debug_printf("read_asset_blob(%s) done len=%zu\n", name, len);
}

static int64_t assets_end_offset(void) {
    int64_t end = 0;
    for (size_t i = 0; i < g_asset_count; i++) {
        int64_t asset_end = g_asset_table[i].offset + g_asset_table[i].length;
        if (asset_end > end)
            end = asset_end;
    }
    return end;
}

static int64_t infer_state_offset(void) {
    int64_t end = assets_end_offset();
    int64_t aligned = (end + 7) & ~INT64_C(7);
    return aligned;
}

static void infer_state_write(const infer_state_header_t *header,
                              const int                  *tokens) {
    int64_t offset = infer_state_offset();
    ic_stable_write(offset, (const uint8_t *)header, (int64_t)sizeof(*header));
    int64_t token_offset = offset + (int64_t)sizeof(*header);
    int64_t token_bytes = (int64_t)header->max_len * (int64_t)sizeof(int);
    ic_stable_write(token_offset, (const uint8_t *)tokens, token_bytes);
}

static void infer_state_read(infer_state_header_t *header, int **tokens) {
    int64_t offset = infer_state_offset();
    ic_stable_read((uint8_t *)header, offset, (int64_t)sizeof(*header));
    if (header->magic != INFER_STATE_MAGIC ||
        header->version != INFER_STATE_VERSION)
        trap_msg("infer state missing or corrupt");
    if (header->max_len == 0 || header->max_len > INT_MAX)
        trap_msg("infer state invalid max_len");
    int *sequence = (int *)malloc((size_t)header->max_len * sizeof(int));
    if (!sequence)
        trap_msg("infer state sequence allocation failed");
    int64_t token_offset = offset + (int64_t)sizeof(*header);
    int64_t token_bytes = (int64_t)header->max_len * (int64_t)sizeof(int);
    ic_stable_read((uint8_t *)sequence, token_offset, token_bytes);
    *tokens = sequence;
}

#ifndef ASSETS_TESTING
static void gpt2_build_from_stable(GPT2 *model, int64_t offset, size_t len) {
    if (len < sizeof(int) * 256)
        trap_msg("checkpoint buffer is too small");
    int model_header[256];
    ic_stable_read((uint8_t *)model_header, offset, sizeof(model_header));
    if (model_header[0] != 20240326)
        trap_msg("bad magic model buffer");
    if (model_header[1] != 1)
        trap_msg("bad version in model buffer");
    size_t num_parameters = gpt2_prepare_param_layout(model, model_header);
    size_t params_bytes = num_parameters * sizeof(float);
    if (len < sizeof(model_header) + params_bytes)
        trap_msg("checkpoint buffer truncated");
    model->params_memory =
        malloc_and_point_parameters(&model->params, model->param_sizes);
    if (!model->params_memory)
        trap_msg("checkpoint parameters allocation failed");
    int64_t params_offset = offset + (int64_t)sizeof(model_header);
    ic_stable_read((uint8_t *)model->params_memory, params_offset,
                   (int64_t)params_bytes);
    model->acts_memory = NULL;
    model->grads_memory = NULL;
    model->m_memory = NULL;
    model->v_memory = NULL;
    model->grads_acts_memory = NULL;
    model->inputs = NULL;
    model->targets = NULL;
    model->batch_size = 0;
    model->seq_len = 0;
    model->mean_loss = -1.0f;
}
#endif

#ifndef ASSETS_TESTING
int llm_c_checkpoint_header_ok(void) {
    asset_entry_t *entry = lookup_asset("checkpoint");
    if (!entry || entry->length < 8)
        return 0;
    uint8_t header[8];
    ic_stable_read(header, entry->offset, 8);
    uint32_t magic = 0;
    uint32_t version = 0;
    memcpy(&magic, header, sizeof(magic));
    memcpy(&version, header + 4, sizeof(version));
    return (magic == 20240326 && version == 1) ? 1 : 0;
}

int llm_c_checkpoint_complete(void) {
    asset_entry_t *entry = lookup_asset("checkpoint");
    if (!entry || entry->length < (int64_t)(sizeof(int) * 256))
        return 0;
    int model_header[256];
    ic_stable_read((uint8_t *)model_header, entry->offset,
                   (int64_t)sizeof(model_header));
    if (model_header[0] != 20240326 || model_header[1] != 1)
        return 0;
    GPT2   model;
    size_t num_parameters = gpt2_prepare_param_layout(&model, model_header);
    size_t params_bytes = num_parameters * sizeof(float);
    size_t expected = sizeof(model_header) + params_bytes;
    if ((uint64_t)entry->length < expected)
        return 0;
    return 1;
}
#endif

void require_assets_loaded(void) {
    ic_api_debug_print("require_assets_loaded() start:\n");
    for (size_t i = 0; i < kRequiredAssetCount; i++) {
        const char *name = kRequiredAssets[i];
        ic_api_debug_printf("require_assets_loaded() checking %s\n", name);
        asset_entry_t *entry = lookup_asset(name);
        if (!entry)
            trap_msg("missing required asset");
        ic_api_debug_printf(
            "require_assets_loaded() confirmed %s offset=%" PRId64
            " len=%" PRId64 "\n",
            name, entry->offset, entry->length);
    }
    ic_api_debug_print("require_assets_loaded() end:\n");
}

#ifndef ASSETS_TESTING
// Compute simple hash of asset data for verification
uint32_t llm_c_compute_asset_hash(const char *name) {
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0) {
        return 0;
    }

    // Simple DJB2 hash algorithm
    uint32_t hash = 5381;
    size_t   len = (size_t)entry->length;

    // Hash in chunks to avoid large memory allocation
    const size_t chunk_size = 65536; // 64KB at a time
    uint8_t     *buffer = (uint8_t *)malloc(chunk_size);
    if (!buffer) {
        return 0;
    }

    int64_t offset = entry->offset;
    while (len > 0) {
        size_t to_read = (len < chunk_size) ? len : chunk_size;
        ic_stable_read(buffer, offset, (int64_t)to_read);

        for (size_t i = 0; i < to_read; i++) {
            hash = ((hash << 5) + hash) + buffer[i]; // hash * 33 + c
        }

        offset += (int64_t)to_read;
        len -= to_read;
    }

    free(buffer);
    return hash;
}

// Read a sample of bytes from asset for verification
void llm_c_get_asset_sample(const char *name,
                            uint32_t    req_offset,
                            uint32_t    req_length,
                            uint8_t   **out_data,
                            size_t     *out_len) {
    asset_entry_t *entry = lookup_asset(name);
    if (!entry || entry->length <= 0) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }

    size_t  asset_len = (size_t)entry->length;
    int64_t byte_offset = entry->offset + (int64_t)req_offset;

    // Clamp to asset bounds
    if (req_offset >= asset_len) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }

    size_t available = asset_len - req_offset;
    size_t to_read = (req_length < available) ? req_length : available;

    uint8_t *buffer = (uint8_t *)malloc(to_read);
    if (!buffer) {
        *out_data = NULL;
        *out_len = 0;
        return;
    }

    ic_stable_read(buffer, byte_offset, (int64_t)to_read);

    *out_data = buffer;
    *out_len = to_read;
}
#endif

#ifndef ASSETS_TESTING
typedef struct {
    float logit;
    int   idx;
} LogitIdx;

static int cmp_desc(const void *a, const void *b) {
    float da = ((LogitIdx *)a)->logit;
    float db = ((LogitIdx *)b)->logit;
    if (da < db)
        return 1;
    if (da > db)
        return -1;
    return 0;
}

static void top_k_filter(float *logits, int n, int k) {
    if (k <= 0 || k >= n)
        return;
    LogitIdx *arr = (LogitIdx *)malloc(sizeof(LogitIdx) * n);
    if (!arr)
        trap_msg("top_k_filter: allocation failure");
    for (int i = 0; i < n; i++) {
        arr[i].logit = logits[i];
        arr[i].idx = i;
    }
    qsort(arr, (size_t)n, sizeof(LogitIdx), cmp_desc);
    float cutoff = arr[k - 1].logit;
    free(arr);
    for (int i = 0; i < n; i++) {
        if (logits[i] < cutoff)
            logits[i] = -INFINITY;
    }
}

static void softmax_vec(float *logits, float *probs, int n) {
    float maxv = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > maxv)
            maxv = logits[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float e = expf(logits[i] - maxv);
        probs[i] = e;
        sum += e;
    }
    float inv = 1.0f / sum;
    for (int i = 0; i < n; i++) {
        probs[i] *= inv;
    }
}

static unsigned int rng_u32(unsigned long long *state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}

static float rng_f32(unsigned long long *state) {
    return (rng_u32(state) >> 8) / 16777216.0f;
}

static int sample_mult_local(float *probabilities, int n, float coin) {
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf)
            return i;
    }
    return n - 1;
}

static void ensure_model_loaded(void);
static int *prepare_sequence_from_tokens(const int *prompt_tokens,
                                         int        prompt_len,
                                         int        max_new,
                                         int        max_seq_len);
static void apply_temperature_and_topk(float *logits, size_t vocab_size);
static int  sample_next_token(unsigned long long *rng_state, size_t vocab_size);
static char *generate_text_cached(int *sequence, int prompt_len, int max_new);

static char *run_inference_internal(const char *prompt, int max_new) {
    ic_api_debug_print("run_inference() start:\n");
    ensure_model_loaded();

    int  prompt_len = 0;
    int *tokens = tokenizer_encode(g_tokenizer, prompt, &prompt_len);
    if (!tokens || prompt_len <= 0) {
        if (tokens)
            free(tokens);
        trap_msg("failed to encode prompt");
    }

    ic_api_debug_printf("prompt encoded len=%d\n", prompt_len);

    int *sequence = prepare_sequence_from_tokens(tokens, prompt_len, max_new,
                                                 g_model.config.max_seq_len);
    free(tokens);

    ic_api_debug_printf("sequence prepared prompt_len=%d alloc_len=%d\n",
                        prompt_len, prompt_len + max_new);

    char *decoded = generate_text_cached(sequence, prompt_len, max_new);
    ic_api_debug_printf("decoded len=%zu\n", strlen(decoded));
    if (prompt_len > 0)
        ic_api_debug_printf("decoded first_token=%d\n", sequence[0]);

    free(sequence);
    return decoded;
}

static void ensure_model_loaded(void) {
    if (g_model_ready && g_tokenizer)
        return;
    require_assets_loaded();

    if (!g_model_ready) {
        int64_t checkpoint_offset = 0;
        int64_t checkpoint_len = 0;
        get_asset_range(kRequiredAssets[0], &checkpoint_offset,
                        &checkpoint_len);
        if (checkpoint_len > (int64_t)SIZE_MAX)
            trap_msg("checkpoint asset too large");
        gpt2_build_from_stable(&g_model, checkpoint_offset,
                               (size_t)checkpoint_len);
        g_vocab_size = (size_t)g_model.config.vocab_size;
        g_work_logits = (float *)malloc(g_vocab_size * sizeof(float));
        g_work_probs = (float *)malloc(g_vocab_size * sizeof(float));
        if (!g_work_logits || !g_work_probs)
            trap_msg("workspace allocation failed");
        g_model_ready = 1;
    }

    if (!g_tokenizer) {
        if (!g_vocab_data) {
            read_asset_blob(kRequiredAssets[1], &g_vocab_data, &g_vocab_len);
            read_asset_blob(kRequiredAssets[2], &g_merges_data, &g_merges_len);
        }
        g_tokenizer = tokenizer_load_from_memory(
            (const char *)g_vocab_data, g_vocab_len,
            (const char *)g_merges_data, g_merges_len);
        if (!g_tokenizer)
            trap_msg("failed to initialize tokenizer");
    }
}

static int *prepare_sequence_from_tokens(const int *prompt_tokens,
                                         int        prompt_len,
                                         int        max_new,
                                         int        max_seq_len) {
    if (prompt_len > max_seq_len)
        trap_msg("prompt exceeds max sequence length");
    if (max_new < 0)
        max_new = 0;
    if (prompt_len + max_new > max_seq_len)
        max_new = max_seq_len - prompt_len;
    if (max_new < 0)
        max_new = 0;
    int alloc_len = prompt_len + max_new;
    if (alloc_len <= 0)
        alloc_len = prompt_len;
    int *sequence = (int *)malloc((size_t)alloc_len * sizeof(int));
    if (!sequence)
        trap_msg("sequence allocation failed");
    memcpy(sequence, prompt_tokens, (size_t)prompt_len * sizeof(int));
    for (int i = prompt_len; i < alloc_len; i++) {
        sequence[i] = sequence[prompt_len - 1];
    }
    return sequence;
}

static void apply_temperature_and_topk(float *logits, size_t vocab_size) {
    if (DEFAULT_TEMPERATURE > 0.0f) {
        float inv_temp = 1.0f / DEFAULT_TEMPERATURE;
        for (size_t i = 0; i < vocab_size; i++) {
            logits[i] *= inv_temp;
        }
    }
    if (DEFAULT_TOP_K > 0) {
        top_k_filter(logits, (int)vocab_size, DEFAULT_TOP_K);
    }
}

static int sample_next_token(unsigned long long *rng_state, size_t vocab_size) {
    softmax_vec(g_work_logits, g_work_probs, (int)vocab_size);
    float coin = rng_f32(rng_state);
    return sample_mult_local(g_work_probs, (int)vocab_size, coin);
}

static char *generate_text_cached(int *sequence, int prompt_len, int max_new) {
    ensure_model_loaded();
    int alloc_len = prompt_len + max_new;
    gpt2_forward(&g_model, sequence, NULL, 1, alloc_len);
    g_model.mean_loss = -1.0f;
    int                seq_len = prompt_len;
    unsigned long long rng_state = 1337;
    ic_api_debug_printf("starting inference loop max_new=%d seq_len=%d\n",
                        max_new, seq_len);
    for (int gen = 0; gen < max_new; gen++) {
        gpt2_forward(&g_model, sequence, NULL, 1, seq_len);
        float *logits = g_model.acts.logits + (seq_len - 1) * (int)g_vocab_size;
        for (size_t i = 0; i < g_vocab_size; i++) {
            g_work_logits[i] = logits[i];
        }
        apply_temperature_and_topk(g_work_logits, g_vocab_size);
        int next = sample_next_token(&rng_state, g_vocab_size);
        sequence[seq_len++] = next;
        ic_api_debug_printf("gen %d token=%d seq_len=%d\n", gen, next, seq_len);
        if (next == GPT2_EOT) {
            break;
        }
    }
    ic_api_debug_printf("generation complete seq_len=%d\n", seq_len);
    char *decoded = tokenizer_decode(g_tokenizer, sequence, seq_len);
    if (!decoded) {
        decoded = duplicate_string("<decode failed>");
    }
    if (!decoded) {
        trap_msg("failed to allocate reply text");
    }
    return decoded;
}

void llm_c_infer_state_init_from_tokens(const int *prompt_tokens,
                                        int        prompt_len,
                                        int        max_new) {
    if (!prompt_tokens || prompt_len <= 0)
        trap_msg("infer_state_init: invalid prompt");
    if (max_new < 0)
        max_new = 0;
    uint32_t max_len = (uint32_t)(prompt_len + max_new);
    if (max_len < (uint32_t)prompt_len)
        trap_msg("infer_state_init: max_len overflow");
    int *sequence = (int *)malloc((size_t)max_len * sizeof(int));
    if (!sequence)
        trap_msg("infer_state_init: sequence allocation failed");
    memcpy(sequence, prompt_tokens, (size_t)prompt_len * sizeof(int));
    if (max_len > (uint32_t)prompt_len) {
        memset(sequence + prompt_len, 0,
               (size_t)(max_len - (uint32_t)prompt_len) * sizeof(int));
    }
    infer_state_header_t header = {
        .magic = INFER_STATE_MAGIC,
        .version = INFER_STATE_VERSION,
        .seq_len = (uint32_t)prompt_len,
        .max_len = max_len,
        .rng_state = 1337ULL,
        .done = 0,
        .reserved = 0,
    };
    infer_state_write(&header, sequence);
    free(sequence);
}

static int generate_single_token_step(int                *sequence,
                                      int                 seq_len,
                                      unsigned long long *rng_state) {
    ensure_model_loaded();
    gpt2_forward(&g_model, sequence, NULL, 1, seq_len);
    float *logits = g_model.acts.logits + (seq_len - 1) * (int)g_vocab_size;
    for (size_t i = 0; i < g_vocab_size; i++) {
        g_work_logits[i] = logits[i];
    }
    apply_temperature_and_topk(g_work_logits, g_vocab_size);
    return sample_next_token(rng_state, g_vocab_size);
}

void llm_c_infer_state_step(uint32_t steps) {
    if (steps == 0) {
        steps = 1;
    }
    infer_state_header_t header;
    int                 *sequence = NULL;
    infer_state_read(&header, &sequence);
    if (header.done) {
        free(sequence);
        return;
    }
    ensure_model_loaded();
    int                 seq_len = (int)header.seq_len;
    int                 max_len = (int)header.max_len;
    unsigned long long *rng_state = &header.rng_state;
    for (uint32_t step = 0; step < steps; step++) {
        if (seq_len >= max_len) {
            header.done = 1;
            break;
        }
        int next = generate_single_token_step(sequence, seq_len, rng_state);
        sequence[seq_len++] = next;
        if (next == GPT2_EOT) {
            header.done = 1;
            break;
        }
    }
    header.seq_len = (uint32_t)seq_len;
    infer_state_write(&header, sequence);
    free(sequence);
}

char *run_inference(const char *prompt) {
    return run_inference_internal(prompt, DEFAULT_MAX_NEW_TOKENS);
}

char *run_inference_limited(const char *prompt, int max_new) {
    return run_inference_internal(prompt, max_new);
}

static char *run_inference_tokens_internal(const int *prompt_tokens,
                                           int        prompt_len,
                                           int        max_new) {
    ic_api_debug_print("run_inference_tokens() start:\n");
    if (!prompt_tokens || prompt_len <= 0)
        trap_msg("run_inference_tokens: invalid prompt tokens");

    ensure_model_loaded();
    ic_api_debug_printf("prompt tokens len=%d\n", prompt_len);

    int *sequence = prepare_sequence_from_tokens(
        prompt_tokens, prompt_len, max_new, g_model.config.max_seq_len);

    char *decoded = generate_text_cached(sequence, prompt_len, max_new);

    free(sequence);
    return decoded;
}

char *
run_inference_tokens(const int *prompt_tokens, int prompt_len, int max_new) {
    return run_inference_tokens_internal(prompt_tokens, prompt_len, max_new);
}
#else
char *run_inference(const char *prompt) {
    (void)prompt;
    return duplicate_string(LLM_C_ASSETS_TESTING_STUB_REPLY);
}

char *run_inference_limited(const char *prompt, int max_new) {
    (void)prompt;
    (void)max_new;
    return duplicate_string(LLM_C_ASSETS_TESTING_STUB_REPLY);
}

char *
run_inference_tokens(const int *prompt_tokens, int prompt_len, int max_new) {
    (void)prompt_tokens;
    (void)prompt_len;
    (void)max_new;
    return duplicate_string(LLM_C_ASSETS_TESTING_STUB_REPLY);
}

void llm_c_infer_state_init_from_tokens(const int *prompt_tokens,
                                        int        prompt_len,
                                        int        max_new) {
    (void)prompt_tokens;
    (void)prompt_len;
    (void)max_new;
    trap_msg("infer_state not supported in asset tests");
}

void llm_c_infer_state_step(uint32_t steps) {
    (void)steps;
    trap_msg("infer_state not supported in asset tests");
}

int read_asset_tokens_u16(const char *name,
                          int         offset,
                          int         count,
                          int       **out_tokens) {
    (void)name;
    (void)offset;
    (void)count;
    (void)out_tokens;
    return 0;
}

int llm_c_asset_exists(const char *name) {
    (void)name;
    return 0;
}

int llm_c_checkpoint_header_ok(void) { return 0; }

int llm_c_checkpoint_complete(void) { return 0; }

uint32_t llm_c_compute_asset_hash(const char *name) {
    (void)name;
    return 0;
}

void llm_c_get_asset_sample(const char *name,
                            uint32_t    req_offset,
                            uint32_t    req_length,
                            uint8_t   **out_data,
                            size_t     *out_len) {
    (void)name;
    (void)req_offset;
    (void)req_length;
    *out_data = NULL;
    *out_len = 0;
}
#endif
