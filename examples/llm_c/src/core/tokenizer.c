#include "tokenizer.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char *gpt_strdup(const char *s) {
    size_t len = strlen(s);
    char  *dst = (char *)malloc(len + 1);
    if (dst) {
        memcpy(dst, s, len + 1);
    }
    return dst;
}

typedef struct {
    char *key;
    int   value;
} StrIntEntry;

typedef struct {
    StrIntEntry *entries;
    int          capacity;
    int          size;
} StrIntMap;

static unsigned long long fnv1a(const char *s) {
    unsigned long long h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)(*s++);
        h *= 1099511628211ull;
    }
    return h;
}

static void map_init(StrIntMap *map, int capacity) {
    map->capacity = capacity;
    map->size = 0;
    map->entries = (StrIntEntry *)calloc((size_t)capacity, sizeof(StrIntEntry));
}

static void map_free(StrIntMap *map) {
    if (!map->entries)
        return;
    for (int i = 0; i < map->capacity; i++) {
        if (map->entries[i].key)
            free(map->entries[i].key);
    }
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->size = 0;
}

static void map_rehash(StrIntMap *map, int new_capacity) {
    StrIntEntry *old = map->entries;
    int          old_cap = map->capacity;
    map_init(map, new_capacity);
    for (int i = 0; i < old_cap; i++) {
        if (old[i].key) {
            unsigned long long h = fnv1a(old[i].key);
            int idx = (int)(h & (unsigned long long)(map->capacity - 1));
            while (map->entries[idx].key) {
                idx = (idx + 1) & (map->capacity - 1);
            }
            map->entries[idx].key = old[i].key;
            map->entries[idx].value = old[i].value;
            map->size++;
        }
    }
    free(old);
}

static void map_put(StrIntMap *map, const char *key, int value) {
    if (map->size * 10 >= map->capacity * 7) {
        map_rehash(map, map->capacity * 2);
    }
    unsigned long long h = fnv1a(key);
    int                idx = (int)(h & (unsigned long long)(map->capacity - 1));
    while (map->entries[idx].key) {
        if (strcmp(map->entries[idx].key, key) == 0) {
            map->entries[idx].value = value;
            return;
        }
        idx = (idx + 1) & (map->capacity - 1);
    }
    map->entries[idx].key = gpt_strdup(key);
    map->entries[idx].value = value;
    map->size++;
}

static int map_get(const StrIntMap *map, const char *key, int *out_value) {
    if (!map->entries)
        return 0;
    unsigned long long h = fnv1a(key);
    int                idx = (int)(h & (unsigned long long)(map->capacity - 1));
    while (map->entries[idx].key) {
        if (strcmp(map->entries[idx].key, key) == 0) {
            if (out_value)
                *out_value = map->entries[idx].value;
            return 1;
        }
        idx = (idx + 1) & (map->capacity - 1);
    }
    return 0;
}

struct GPT2Tokenizer {
    char    **id_to_token;
    int       vocab_size;
    StrIntMap token_to_id;
    StrIntMap merges;
    char    **byte_encoder;
    int      *codepoint_to_byte;
    int       codepoint_cap;
};

static int utf8_encode(int cp, char *out) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

static int utf8_decode(const char *s, int *cp) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) {
        *cp = c;
        return 1;
    } else if ((c >> 5) == 0x6) {
        *cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    } else if ((c >> 4) == 0xE) {
        *cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    } else {
        *cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
              ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
}

static char *parse_json_string(const char **p) {
    const char *s = *p;
    if (*s != '"')
        return NULL;
    s++;
    size_t cap = 64;
    size_t len = 0;
    char  *out = (char *)malloc(cap);
    if (!out)
        return NULL;
    while (*s && *s != '"') {
        char ch = *s++;
        if (ch == '\\') {
            char esc = *s++;
            if (esc == 'n')
                ch = '\n';
            else if (esc == 'r')
                ch = '\r';
            else if (esc == 't')
                ch = '\t';
            else if (esc == 'b')
                ch = '\b';
            else if (esc == 'f')
                ch = '\f';
            else if (esc == '"' || esc == '\\' || esc == '/')
                ch = esc;
            else if (esc == 'u') {
                int code = 0;
                for (int i = 0; i < 4; i++) {
                    char c = *s++;
                    code <<= 4;
                    if (c >= '0' && c <= '9')
                        code |= c - '0';
                    else if (c >= 'a' && c <= 'f')
                        code |= c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        code |= c - 'A' + 10;
                }
                char buf[4];
                int  n = utf8_encode(code, buf);
                if (len + (size_t)n + 1 > cap) {
                    cap *= 2;
                    out = (char *)realloc(out, cap);
                }
                for (int i = 0; i < n; i++)
                    out[len++] = buf[i];
                continue;
            }
        }
        if (len + 2 > cap) {
            cap *= 2;
            out = (char *)realloc(out, cap);
        }
        out[len++] = ch;
    }
    out[len] = '\0';
    if (*s == '"')
        s++;
    *p = s;
    return out;
}

static int parse_vocab_json_data(GPT2Tokenizer *tok, char *data) {
    const char *p = data;
    while (*p && *p != '{')
        p++;
    if (*p != '{')
        return 0;
    p++;
    int cap = 60000;
    tok->id_to_token = (char **)calloc((size_t)cap, sizeof(char *));
    tok->vocab_size = 0;
    map_init(&tok->token_to_id, 1 << 17);
    while (*p) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '}')
            break;
        char *key = parse_json_string(&p);
        if (!key)
            break;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
                      *p == ':'))
            p++;
        int id = (int)strtol(p, (char **)&p, 10);
        if (id >= cap) {
            int new_cap = cap;
            while (new_cap <= id)
                new_cap *= 2;
            tok->id_to_token = (char **)realloc(
                tok->id_to_token, (size_t)new_cap * sizeof(char *));
            for (int i = cap; i < new_cap; i++)
                tok->id_to_token[i] = NULL;
            cap = new_cap;
        }
        tok->id_to_token[id] = key;
        if (id + 1 > tok->vocab_size)
            tok->vocab_size = id + 1;
        map_put(&tok->token_to_id, key, id);
        while (*p && *p != ',' && *p != '}')
            p++;
        if (*p == ',')
            p++;
    }
    return 1;
}

static char *make_pair_key(const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char  *key = (char *)malloc(la + lb + 2);
    memcpy(key, a, la);
    key[la] = 0x1F;
    memcpy(key + la + 1, b, lb);
    key[la + 1 + lb] = '\0';
    return key;
}

static int parse_merges_data(GPT2Tokenizer *tok, char *data) {
    map_init(&tok->merges, 1 << 17);
    int   rank = 0;
    char *line = data;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next)
            *next++ = '\0';
        if (line[0] == '#' || line[0] == '\0') {
            line = next;
            continue;
        }
        char *space = strchr(line, ' ');
        if (!space) {
            line = next;
            continue;
        }
        *space = '\0';
        char *a = line;
        char *b = space + 1;
        if (*a && *b) {
            char *key = make_pair_key(a, b);
            map_put(&tok->merges, key, rank++);
            free(key);
        }
        line = next;
    }
    return 1;
}

static void build_byte_encoder(GPT2Tokenizer *tok) {
    tok->byte_encoder = (char **)malloc(256 * sizeof(char *));
    int used[256];
    for (int i = 0; i < 256; i++)
        used[i] = 0;
    int list[256];
    int list_len = 0;
    for (int i = 33; i <= 126; i++)
        list[list_len++] = i;
    for (int i = 161; i <= 172; i++)
        list[list_len++] = i;
    for (int i = 174; i <= 255; i++)
        list[list_len++] = i;
    for (int i = 0; i < list_len; i++)
        used[list[i]] = 1;
    int extra = 0;
    int max_cp = 0;
    for (int b = 0; b < 256; b++) {
        int cp = used[b] ? b : (256 + extra++);
        if (cp > max_cp)
            max_cp = cp;
        char  buf[4];
        int   n = utf8_encode(cp, buf);
        char *s = (char *)malloc((size_t)n + 1);
        for (int i = 0; i < n; i++)
            s[i] = buf[i];
        s[n] = '\0';
        tok->byte_encoder[b] = s;
    }
    tok->codepoint_cap = max_cp + 1;
    tok->codepoint_to_byte =
        (int *)malloc((size_t)tok->codepoint_cap * sizeof(int));
    for (int i = 0; i < tok->codepoint_cap; i++)
        tok->codepoint_to_byte[i] = -1;
    for (int b = 0; b < 256; b++) {
        int cp = 0;
        utf8_decode(tok->byte_encoder[b], &cp);
        if (cp >= 0 && cp < tok->codepoint_cap)
            tok->codepoint_to_byte[cp] = b;
    }
}

static int is_alpha(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int is_digit(unsigned char c) { return (c >= '0' && c <= '9'); }

static char **
bpe(GPT2Tokenizer *tok, const unsigned char *bytes, int len, int *out_count) {
    char **symbols = (char **)malloc((size_t)len * sizeof(char *));
    for (int i = 0; i < len; i++) {
        symbols[i] = gpt_strdup(tok->byte_encoder[bytes[i]]);
    }
    int count = len;
    if (count == 1) {
        *out_count = count;
        return symbols;
    }
    while (count > 1) {
        int best_rank = INT_MAX;
        int best_i = -1;
        for (int i = 0; i < count - 1; i++) {
            char *key = make_pair_key(symbols[i], symbols[i + 1]);
            int   rank = 0;
            if (map_get(&tok->merges, key, &rank)) {
                if (rank < best_rank) {
                    best_rank = rank;
                    best_i = i;
                }
            }
            free(key);
        }
        if (best_i < 0)
            break;
        size_t la = strlen(symbols[best_i]);
        size_t lb = strlen(symbols[best_i + 1]);
        char  *merged = (char *)malloc(la + lb + 1);
        memcpy(merged, symbols[best_i], la);
        memcpy(merged + la, symbols[best_i + 1], lb);
        merged[la + lb] = '\0';
        free(symbols[best_i]);
        free(symbols[best_i + 1]);
        symbols[best_i] = merged;
        for (int i = best_i + 1; i < count - 1; i++) {
            symbols[i] = symbols[i + 1];
        }
        count--;
    }
    *out_count = count;
    return symbols;
}

static char *duplicate_buffer(const char *data, size_t len) {
    char *copy = (char *)malloc(len + 1);
    if (!copy)
        return NULL;
    memcpy(copy, data, len);
    copy[len] = '\0';
    return copy;
}

// Forward declarations
static void gpt2_tokenizer_free_impl(GPT2Tokenizer *tok);
static int *
gpt2_tokenizer_encode_impl(GPT2Tokenizer *tok, const char *text, int *out_len);
static char *gpt2_tokenizer_decode_impl(GPT2Tokenizer *tok,
                                        const int     *token_ids,
                                        int            num_tokens);

static GPT2Tokenizer *
gpt2_tokenizer_load_from_memory_impl(const char *vocab_data,
                                     size_t      vocab_len,
                                     const char *merges_data,
                                     size_t      merges_len) {
    if (vocab_len == 0 || merges_len == 0 || vocab_data == NULL ||
        merges_data == NULL) {
        return NULL;
    }
    GPT2Tokenizer *tok = (GPT2Tokenizer *)calloc(1, sizeof(GPT2Tokenizer));
    if (!tok)
        return NULL;
    char *vocab_copy = duplicate_buffer(vocab_data, vocab_len);
    if (!vocab_copy)
        goto fail;
    if (!parse_vocab_json_data(tok, vocab_copy)) {
        free(vocab_copy);
        goto fail;
    }
    free(vocab_copy);
    char *merges_copy = duplicate_buffer(merges_data, merges_len);
    if (!merges_copy)
        goto fail;
    if (!parse_merges_data(tok, merges_copy)) {
        free(merges_copy);
        goto fail;
    }
    free(merges_copy);
    build_byte_encoder(tok);
    return tok;
fail:
    if (tok) {
        gpt2_tokenizer_free_impl(tok);
    }
    return NULL;
}

static void gpt2_tokenizer_free_impl(GPT2Tokenizer *tok) {
    if (!tok)
        return;
    if (tok->id_to_token) {
        for (int i = 0; i < tok->vocab_size; i++) {
            if (tok->id_to_token[i])
                free(tok->id_to_token[i]);
        }
        free(tok->id_to_token);
    }
    map_free(&tok->token_to_id);
    map_free(&tok->merges);
    if (tok->byte_encoder) {
        for (int i = 0; i < 256; i++)
            free(tok->byte_encoder[i]);
        free(tok->byte_encoder);
    }
    if (tok->codepoint_to_byte)
        free(tok->codepoint_to_byte);
    free(tok);
}

static int *
gpt2_tokenizer_encode_impl(GPT2Tokenizer *tok, const char *text, int *out_len) {
    const unsigned char *bytes = (const unsigned char *)text;
    int                  n = (int)strlen(text);
    int                  cap = n * 2 + 8;
    int                 *out = (int *)malloc((size_t)cap * sizeof(int));
    int                  out_count = 0;

    int i = 0;
    while (i < n) {
        if (bytes[i] == ' ') {
            int j = i;
            while (j < n && bytes[j] == ' ')
                j++;
            int spaces = j - i;
            if (spaces == 1 && j < n) {
                i = i;
            } else {
                int    bpe_count = 0;
                char **bpe_syms = bpe(tok, bytes + i, spaces, &bpe_count);
                for (int k = 0; k < bpe_count; k++) {
                    int id = -1;
                    if (!map_get(&tok->token_to_id, bpe_syms[k], &id))
                        id = -1;
                    if (id >= 0) {
                        if (out_count >= cap) {
                            cap *= 2;
                            out =
                                (int *)realloc(out, (size_t)cap * sizeof(int));
                        }
                        out[out_count++] = id;
                    }
                    free(bpe_syms[k]);
                }
                free(bpe_syms);
                i = j;
                continue;
            }
        }

        int start = i;
        int j = i;
        if (bytes[j] == ' ')
            j++;
        if (j < n && is_alpha(bytes[j])) {
            j++;
            while (j < n && is_alpha(bytes[j]))
                j++;
        } else if (j < n && is_digit(bytes[j])) {
            j++;
            while (j < n && is_digit(bytes[j]))
                j++;
        } else {
            if (j < n)
                j++;
            while (j < n && bytes[j] != ' ' && !is_alpha(bytes[j]) &&
                   !is_digit(bytes[j]))
                j++;
        }

        int    span_len = j - start;
        int    bpe_count = 0;
        char **bpe_syms = bpe(tok, bytes + start, span_len, &bpe_count);
        for (int k = 0; k < bpe_count; k++) {
            int id = -1;
            if (!map_get(&tok->token_to_id, bpe_syms[k], &id))
                id = -1;
            if (id >= 0) {
                if (out_count >= cap) {
                    cap *= 2;
                    out = (int *)realloc(out, (size_t)cap * sizeof(int));
                }
                out[out_count++] = id;
            }
            free(bpe_syms[k]);
        }
        free(bpe_syms);
        i = j;
    }
    *out_len = out_count;
    return out;
}

static char *gpt2_tokenizer_decode_impl(GPT2Tokenizer *tok,
                                        const int     *token_ids,
                                        int            num_tokens) {
    size_t         cap = 256;
    size_t         len = 0;
    unsigned char *bytes = (unsigned char *)malloc(cap);
    for (int i = 0; i < num_tokens; i++) {
        int id = token_ids[i];
        if (id < 0 || id >= tok->vocab_size)
            continue;
        const char *tok_str = tok->id_to_token[id];
        if (!tok_str)
            continue;
        const char *p = tok_str;
        while (*p) {
            int cp = 0;
            int n = utf8_decode(p, &cp);
            p += n;
            int b = (cp >= 0 && cp < tok->codepoint_cap)
                        ? tok->codepoint_to_byte[cp]
                        : -1;
            if (b < 0)
                continue;
            if (len + 1 >= cap) {
                cap *= 2;
                bytes = (unsigned char *)realloc(bytes, cap);
            }
            bytes[len++] = (unsigned char)b;
        }
    }
    bytes[len] = '\0';
    return (char *)bytes;
}

// Public API wrappers (use platform allocator)

GPT2Tokenizer *gpt2_tokenizer_load_from_memory(const char *vocab_data,
                                               size_t      vocab_len,
                                               const char *merges_data,
                                               size_t      merges_len,
                                               void *(*alloc_fn)(size_t)) {
    // For now, use malloc internally (will be refactored)
    // The alloc_fn parameter is for future use when we refactor internal
    // allocations
    (void)alloc_fn; // Suppress unused parameter warning
    return gpt2_tokenizer_load_from_memory_impl(vocab_data, vocab_len,
                                                merges_data, merges_len);
}

void gpt2_tokenizer_free(GPT2Tokenizer *tok, void (*free_fn)(void *)) {
    // For now, use free internally (will be refactored)
    (void)free_fn; // Suppress unused parameter warning
    gpt2_tokenizer_free_impl(tok);
}

int *gpt2_tokenizer_encode(GPT2Tokenizer *tok,
                           const char    *text,
                           int           *out_len,
                           void *(*alloc_fn)(size_t)) {
    // For now, use malloc internally (will be refactored)
    (void)alloc_fn; // Suppress unused parameter warning
    return gpt2_tokenizer_encode_impl(tok, text, out_len);
}

char *gpt2_tokenizer_decode(GPT2Tokenizer *tok,
                            const int     *token_ids,
                            int            num_tokens,
                            void *(*alloc_fn)(size_t)) {
    // For now, use malloc internally (will be refactored)
    (void)alloc_fn; // Suppress unused parameter warning
    return gpt2_tokenizer_decode_impl(tok, token_ids, num_tokens);
}
