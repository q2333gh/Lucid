// Minimal GPT-2 inference runner.
// Builds on the training code by including it with TESTING defined (skips the
// training main).
#define TESTING
#include "../model/train_gpt2.c"

#include "../model/tokenizer.h"
#include <string.h>

#define GPT2_EOT 50256

typedef struct {
    float       temperature;
    int         top_k;
    int         max_new_tokens;
    const char *checkpoint_path;
    const char *vocab_path;
    const char *merges_path;
    const char *prompt;
    int         decode;
} InferenceConfig;

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
    for (int i = 0; i < n; i++) {
        arr[i].logit = logits[i];
        arr[i].idx = i;
    }
    qsort(arr, n, sizeof(LogitIdx), cmp_desc);
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

static int parse_tokens(const char *token_str, int **out_tokens) {
    // token_str is space-separated ints
    char *copy = strdup(token_str);
    if (copy == NULL)
        return -1;
    int count = 0;
    for (char *p = copy; *p; p++) {
        if (*p == ' ')
            count++;
    }
    count += 1; // number of tokens = spaces + 1
    int *tokens = (int *)malloc(sizeof(int) * count);
    if (tokens == NULL) {
        free(copy);
        return -1;
    }
    int   idx = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(copy, " ", &saveptr);
    while (tok != NULL && idx < count) {
        tokens[idx++] = atoi(tok);
        tok = strtok_r(NULL, " ", &saveptr);
    }
    free(copy);
    *out_tokens = tokens;
    return idx;
}

static void usage() {
    printf("Usage: ./run_gpt2 [--tokens \"<space separated token ids>\"] "
           "[--prompt \"text\"] [--decode]\n");
    printf(
        "                [--max-new N] [--temp T] [--top-k K] [--ckpt path]\n");
    printf("                [--vocab path] [--merges path]\n");
    printf("Example: ./run_gpt2 --prompt \"Hello\" --max-new 32 --temp 1.0 "
           "--top-k 40\n");
}

int main(int argc, char **argv) {
    InferenceConfig cfg = {
        .temperature = 1.0f,
        .top_k = 40,
        .max_new_tokens = 64,
        .checkpoint_path = "gpt2_124M.bin",
        .vocab_path = "vocab/encoder.json",
        .merges_path = "vocab/vocab.bpe",
        .prompt = NULL,
        .decode = 0,
    };
    const char *tokens_arg = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0 && i + 1 < argc) {
            tokens_arg = argv[++i];
        } else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) {
            cfg.prompt = argv[++i];
        } else if (strcmp(argv[i], "--max-new") == 0 && i + 1 < argc) {
            cfg.max_new_tokens = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--temp") == 0 && i + 1 < argc) {
            cfg.temperature = atof(argv[++i]);
        } else if (strcmp(argv[i], "--top-k") == 0 && i + 1 < argc) {
            cfg.top_k = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ckpt") == 0 && i + 1 < argc) {
            cfg.checkpoint_path = argv[++i];
        } else if (strcmp(argv[i], "--vocab") == 0 && i + 1 < argc) {
            cfg.vocab_path = argv[++i];
        } else if (strcmp(argv[i], "--merges") == 0 && i + 1 < argc) {
            cfg.merges_path = argv[++i];
        } else if (strcmp(argv[i], "--decode") == 0) {
            cfg.decode = 1;
        } else {
            usage();
            return 1;
        }
    }

    if (tokens_arg == NULL && cfg.prompt == NULL) {
        printf("Error: --tokens or --prompt is required\n");
        usage();
        return 1;
    }

    GPT2Tokenizer *tok = NULL;
    int           *tokens = NULL;
    int            prompt_len = 0;
    if (cfg.prompt != NULL || cfg.decode) {
        tok = tokenizer_load(cfg.vocab_path, cfg.merges_path);
        if (tok == NULL) {
            printf("Error: failed to load vocab/merges\n");
            return 1;
        }
    }
    if (cfg.prompt != NULL) {
        tokens = tokenizer_encode(tok, cfg.prompt, &prompt_len);
    } else {
        prompt_len = parse_tokens(tokens_arg, &tokens);
    }
    if (prompt_len <= 0 || tokens == NULL) {
        printf("Error: failed to parse tokens\n");
        if (tok)
            tokenizer_free(tok);
        return 1;
    }

    GPT2 model;
    gpt2_build_from_checkpoint(&model, (char *)cfg.checkpoint_path);
    int max_seq = model.config.max_seq_len;
    int vocab = model.config.vocab_size;
    int total_needed = prompt_len + cfg.max_new_tokens;
    if (total_needed > max_seq) {
        printf("Warning: truncating generation to max_seq_len=%d\n", max_seq);
        cfg.max_new_tokens = max_seq - prompt_len;
    }

    int *sequence =
        (int *)malloc(sizeof(int) * (prompt_len + cfg.max_new_tokens));
    memcpy(sequence, tokens, sizeof(int) * prompt_len);
    free(tokens);

    unsigned long long rng_state = 1337;
    float             *work_logits = (float *)malloc(sizeof(float) * vocab);
    float             *work_probs = (float *)malloc(sizeof(float) * vocab);
    if (work_logits == NULL || work_probs == NULL || sequence == NULL) {
        printf("Error: memory allocation failed\n");
        if (tok)
            tokenizer_free(tok);
        return 1;
    }

    int alloc_len = prompt_len + cfg.max_new_tokens;
    for (int i = prompt_len; i < alloc_len; i++) {
        sequence[i] = sequence[prompt_len - 1];
    }
    gpt2_forward(&model, sequence, NULL, 1,
                 alloc_len); // allocate buffers up front
    model.mean_loss = -1.0f; // clear loss flag

    int seq_len = prompt_len;
    for (int gen = 0; gen < cfg.max_new_tokens; gen++) {
        gpt2_forward(&model, sequence, NULL, 1, seq_len);
        float *logits = model.acts.logits + (seq_len - 1) * vocab;
        for (int i = 0; i < vocab; i++) {
            work_logits[i] = logits[i];
        }
        if (cfg.temperature > 0.0f) {
            float inv_temp = 1.0f / cfg.temperature;
            for (int i = 0; i < vocab; i++)
                work_logits[i] *= inv_temp;
        }
        if (cfg.top_k > 0)
            top_k_filter(work_logits, vocab, cfg.top_k);
        softmax_vec(work_logits, work_probs, vocab);
        float coin = rng_f32(&rng_state);
        int   next = sample_mult_local(work_probs, vocab, coin);
        sequence[seq_len++] = next;
        if (next == GPT2_EOT)
            break;
    }

    printf("Generated token ids:\n");
    for (int i = 0; i < seq_len; i++) {
        printf("%d", sequence[i]);
        if (i + 1 < seq_len)
            printf(" ");
    }
    printf("\n");

    if (tok) {
        char *text = tokenizer_decode(tok, sequence, seq_len);
        if (text) {
            printf("Decoded text:\n%s\n", text);
            free(text);
        }
        tokenizer_free(tok);
    }

    free(sequence);
    free(work_logits);
    free(work_probs);
    gpt2_free(&model);
    return 0;
}
