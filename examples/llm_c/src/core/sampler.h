#ifndef LLM_C_CORE_SAMPLER_H
#define LLM_C_CORE_SAMPLER_H

#include <stdint.h>

/**
 * Sampling functions for token generation (platform-independent)
 */

/**
 * Apply top-k filtering to logits.
 * Sets logits below the k-th highest to -INFINITY.
 *
 * @param logits Array of logits (modified in-place)
 * @param n Number of logits
 * @param k Top-k value
 */
void gpt2_top_k_filter(float *logits, int n, int k);

/**
 * Compute softmax over a vector.
 *
 * @param logits Input logits
 * @param probs Output probabilities (must be allocated)
 * @param n Number of elements
 */
void gpt2_softmax_vec(float *logits, float *probs, int n);

/**
 * Sample from a probability distribution using CDF.
 *
 * @param probabilities Array of probabilities (must sum to 1.0)
 * @param n Number of probabilities
 * @param coin Random value in [0, 1)
 * @return Sampled index
 */
int gpt2_sample_mult(float *probabilities, int n, float coin);

/**
 * Generate random uint32 from RNG state.
 * Uses xorshift algorithm.
 *
 * @param state RNG state (modified)
 * @return Random uint32
 */
unsigned int gpt2_rng_u32(unsigned long long *state);

/**
 * Generate random float32 in [0, 1) from RNG state.
 *
 * @param state RNG state (modified)
 * @return Random float in [0, 1)
 */
float gpt2_rng_f32(unsigned long long *state);

#endif // LLM_C_CORE_SAMPLER_H
