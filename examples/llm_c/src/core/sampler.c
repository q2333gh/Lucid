/*
 * GPT-2 Sampler Core - Platform Independent
 *
 * Sampling functions for token generation: top-k filtering, softmax, RNG.
 */

#include "sampler.h"
#include <math.h>
#include <stdlib.h>

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

void gpt2_top_k_filter(float *logits, int n, int k) {
    if (k <= 0 || k >= n)
        return;
    LogitIdx *arr = (LogitIdx *)malloc(sizeof(LogitIdx) * n);
    if (!arr)
        return;
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

void gpt2_softmax_vec(float *logits, float *probs, int n) {
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

unsigned int gpt2_rng_u32(unsigned long long *state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}

float gpt2_rng_f32(unsigned long long *state) {
    return (gpt2_rng_u32(state) >> 8) / 16777216.0f;
}

int gpt2_sample_mult(float *probabilities, int n, float coin) {
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf)
            return i;
    }
    return n - 1; // in case of rounding errors
}
