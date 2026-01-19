#ifndef LLM_C_CORE_MODEL_H
#define LLM_C_CORE_MODEL_H

#include <stddef.h>
#include <stdint.h>

// Forward pass functions (platform-independent)
void encoder_forward(
    float *out, int *inp, float *wte, float *wpe, int B, int T, int C);
void layernorm_forward(float *out,
                       float *mean,
                       float *rstd,
                       float *inp,
                       float *weight,
                       float *bias,
                       int    B,
                       int    T,
                       int    C);
void matmul_forward(float *out,
                    float *inp,
                    float *weight,
                    float *bias,
                    int    B,
                    int    T,
                    int    C,
                    int    OC);
void attention_forward(float *out,
                       float *preatt,
                       float *att,
                       float *inp,
                       int    B,
                       int    T,
                       int    C,
                       int    NH);
void gelu_forward(float *out, float *inp, int N);
void residual_forward(float *out, float *inp1, float *inp2, int N);
void softmax_forward(float *probs, float *logits, int B, int T, int V);
void crossentropy_forward(
    float *losses, float *probs, int *targets, int B, int T, int V);

// GPT-2 model structures
#define NUM_PARAMETER_TENSORS 16
typedef struct {
    float *wte;      // (V, C)
    float *wpe;      // (maxT, C)
    float *ln1w;     // (L, C)
    float *ln1b;     // (L, C)
    float *qkvw;     // (L, 3*C, C)
    float *qkvb;     // (L, 3*C)
    float *attprojw; // (L, C, C)
    float *attprojb; // (L, C)
    float *ln2w;     // (L, C)
    float *ln2b;     // (L, C)
    float *fcw;      // (L, 4*C, C)
    float *fcb;      // (L, 4*C)
    float *fcprojw;  // (L, C, 4*C)
    float *fcprojb;  // (L, C)
    float *lnfw;     // (C)
    float *lnfb;     // (C)
} ParameterTensors;

#define NUM_ACTIVATION_TENSORS 25
typedef struct {
    float *encoded;     // (B, T, C)
    float *ln1;         // (L, B, T, C)
    float *ln1_mean;    // (L, B, T)
    float *ln1_rstd;    // (L, B, T)
    float *qkv;         // (L, B, T, 3*C)
    float *atty;        // (L, B, T, C)
    float *preatt;      // (L, B, NH, T, T)
    float *att;         // (L, B, NH, T, T)
    float *attproj;     // (L, B, T, C)
    float *residual2;   // (L, B, T, C)
    float *ln2;         // (L, B, T, C)
    float *ln2_mean;    // (L, B, T)
    float *ln2_rstd;    // (L, B, T)
    float *fch;         // (L, B, T, 4*C)
    float *fch_gelu;    // (L, B, T, 4*C)
    float *fcproj;      // (L, B, T, C)
    float *residual3;   // (L, B, T, C)
    float *lnf;         // (B, T, C)
    float *lnf_mean;    // (B, T)
    float *lnf_rstd;    // (B, T)
    float *logits;      // (B, T, V)
    float *probs;       // (B, T, V)
    float *losses;      // (B, T)
    float *key_cache;   // (L, maxT, C)
    float *value_cache; // (L, maxT, C)
} ActivationTensors;

typedef struct {
    int max_seq_len; // max sequence length, e.g. 1024
    int vocab_size;  // vocab size, e.g. 50257
    int num_layers;  // number of layers, e.g. 12
    int num_heads;   // number of heads in attention, e.g. 12
    int channels;    // number of channels, e.g. 768
} GPT2Config;

typedef struct {
    GPT2Config config;
    // the weights of the model, and their sizes
    ParameterTensors params;
    size_t           param_sizes[NUM_PARAMETER_TENSORS];
    float           *params_memory;
    int              num_parameters;
    // gradients of the weights (for training, unused in inference)
    ParameterTensors grads;
    float           *grads_memory;
    // buffers for the AdamW optimizer (for training, unused in inference)
    float *m_memory;
    float *v_memory;
    // the activations of the model, and their sizes
    ActivationTensors acts;
    size_t            act_sizes[NUM_ACTIVATION_TENSORS];
    float            *acts_memory;
    int               num_activations;
    // gradients of the activations (for training, unused in inference)
    ActivationTensors grads_acts;
    float            *grads_acts_memory;
    // other run state configuration
    int   batch_size; // the batch size (B) of current forward pass
    int   seq_len;    // the sequence length (T) of current forward pass
    int  *inputs;     // the input tokens for the current forward pass
    int  *targets; // the target tokens for the current forward pass (optional)
    float mean_loss; // after a forward pass with targets, will be populated
                     // with the mean loss
    int cache_pos;   // current position in KV cache
} GPT2;

// Parameter layout and memory management
float *gpt2_malloc_and_point_parameters(ParameterTensors *params,
                                        size_t           *param_sizes,
                                        void *(*alloc_fn)(size_t));
float *gpt2_malloc_and_point_activations(ActivationTensors *acts,
                                         size_t            *act_sizes,
                                         void *(*alloc_fn)(size_t));

// Model initialization from buffer (platform-independent)
size_t gpt2_prepare_param_layout(GPT2 *model, const int *model_header);
void   gpt2_build_from_buffer(GPT2          *model,
                              const uint8_t *buffer,
                              size_t         len,
                              void *(*alloc_fn)(size_t),
                              void (*error_fn)(const char *));

// Forward pass (inference)
void gpt2_forward(GPT2 *model,
                  int  *inputs,
                  int  *targets,
                  int   B,
                  int   T,
                  void (*error_fn)(const char *));
void gpt2_forward_single(GPT2 *model,
                         int   token,
                         int   pos,
                         void (*error_fn)(const char *));

// Cleanup
void gpt2_free(GPT2 *model, void (*free_fn)(void *));

#endif // LLM_C_CORE_MODEL_H
