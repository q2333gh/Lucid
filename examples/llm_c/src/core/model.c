/*
 * GPT-2 Model Core - Platform Independent
 *
 * This file contains the core GPT-2 model implementation that is
 * platform-independent. All platform-specific operations (I/O, memory,
 * error handling) are abstracted through function pointers.
 */

#include "model.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Forward pass functions (platform-independent)

void encoder_forward(
    float *out, int *inp, float *wte, float *wpe, int B, int T, int C) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float *out_bt = out + b * T * C + t * C;
            int    ix = inp[b * T + t];
            float *wte_ix = wte + ix * C;
            float *wpe_t = wpe + t * C;
            for (int i = 0; i < C; i++) {
                out_bt[i] = wte_ix[i] + wpe_t[i];
            }
        }
    }
}

void layernorm_forward(float *out,
                       float *mean,
                       float *rstd,
                       float *inp,
                       float *weight,
                       float *bias,
                       int    B,
                       int    T,
                       int    C) {
    float eps = 1e-5f;
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float *x = inp + b * T * C + t * C;
            float  m = 0.0f;
            for (int i = 0; i < C; i++) {
                m += x[i];
            }
            m = m / C;
            float v = 0.0f;
            for (int i = 0; i < C; i++) {
                float xshift = x[i] - m;
                v += xshift * xshift;
            }
            v = v / C;
            float  s = 1.0f / sqrtf(v + eps);
            float *out_bt = out + b * T * C + t * C;
            for (int i = 0; i < C; i++) {
                float n = (s * (x[i] - m));
                float o = n * weight[i] + bias[i];
                out_bt[i] = o;
            }
            mean[b * T + t] = m;
            rstd[b * T + t] = s;
        }
    }
}

void        matmul_forward(float *out,
                           float *inp,
                           float *weight,
                           float *bias,
                           int    B,
                           int    T,
                           int    C,
                           int    OC) {
#pragma omp parallel for collapse(2)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float *out_bt = out + b * T * OC + t * OC;
            float *inp_bt = inp + b * T * C + t * C;
            for (int o = 0; o < OC; o++) {
                float  val = (bias != NULL) ? bias[o] : 0.0f;
                float *wrow = weight + o * C;
                for (int i = 0; i < C; i++) {
                    val += inp_bt[i] * wrow[i];
                }
                out_bt[o] = val;
            }
        }
    }
}

void attention_forward(float *out,
                       float *preatt,
                       float *att,
                       float *inp,
                       int    B,
                       int    T,
                       int    C,
                       int    NH) {
    int   C3 = C * 3;
    int   hs = C / NH;
    float scale = 1.0 / sqrtf(hs);

#pragma omp parallel for collapse(3)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                float *query_t = inp + b * T * C3 + t * C3 + h * hs;
                float *preatt_bth = preatt + b * NH * T * T + h * T * T + t * T;
                float *att_bth = att + b * NH * T * T + h * T * T + t * T;

                float maxval = -10000.0f;
                for (int t2 = 0; t2 <= t; t2++) {
                    float *key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C;
                    float  val = 0.0f;
                    for (int i = 0; i < hs; i++) {
                        val += query_t[i] * key_t2[i];
                    }
                    val *= scale;
                    if (val > maxval) {
                        maxval = val;
                    }
                    preatt_bth[t2] = val;
                }

                float expsum = 0.0f;
                for (int t2 = 0; t2 <= t; t2++) {
                    float expv = expf(preatt_bth[t2] - maxval);
                    expsum += expv;
                    att_bth[t2] = expv;
                }
                float expsum_inv = expsum == 0.0f ? 0.0f : 1.0f / expsum;

                for (int t2 = 0; t2 < T; t2++) {
                    if (t2 <= t) {
                        att_bth[t2] *= expsum_inv;
                    } else {
                        att_bth[t2] = 0.0f;
                    }
                }

                float *out_bth = out + b * T * C + t * C + h * hs;
                for (int i = 0; i < hs; i++) {
                    out_bth[i] = 0.0f;
                }
                for (int t2 = 0; t2 <= t; t2++) {
                    float *value_t2 =
                        inp + b * T * C3 + t2 * C3 + h * hs + C * 2;
                    float att_btht2 = att_bth[t2];
                    for (int i = 0; i < hs; i++) {
                        out_bth[i] += att_btht2 * value_t2[i];
                    }
                }
            }
        }
    }
}

void gelu_forward(float *out, float *inp, int N) {
    float s = sqrtf(2.0f / M_PI);
    for (int i = 0; i < N; i++) {
        float x = inp[i];
        float cube = 0.044715f * x * x * x;
        out[i] = 0.5f * x * (1.0f + tanhf(s * (x + cube)));
    }
}

void residual_forward(float *out, float *inp1, float *inp2, int N) {
    for (int i = 0; i < N; i++) {
        out[i] = inp1[i] + inp2[i];
    }
}

void        softmax_forward(float *probs, float *logits, int B, int T, int V) {
#pragma omp parallel for collapse(2)
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float *logits_bt = logits + b * T * V + t * V;
            float *probs_bt = probs + b * T * V + t * V;

            float maxval = -10000.0f;
            for (int i = 0; i < V; i++) {
                if (logits_bt[i] > maxval) {
                    maxval = logits_bt[i];
                }
            }
            float sum = 0.0f;
            for (int i = 0; i < V; i++) {
                probs_bt[i] = expf(logits_bt[i] - maxval);
                sum += probs_bt[i];
            }
            for (int i = 0; i < V; i++) {
                probs_bt[i] /= sum;
            }
        }
    }
}

void crossentropy_forward(
    float *losses, float *probs, int *targets, int B, int T, int V) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float *probs_bt = probs + b * T * V + t * V;
            int    ix = targets[b * T + t];
            losses[b * T + t] = -logf(probs_bt[ix]);
        }
    }
}

// Parameter layout and memory management

float *gpt2_malloc_and_point_parameters(ParameterTensors *params,
                                        size_t           *param_sizes,
                                        void *(*alloc_fn)(size_t)) {
    size_t num_parameters = 0;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        num_parameters += param_sizes[i];
    }
    float *params_memory = (float *)alloc_fn(num_parameters * sizeof(float));
    if (!params_memory)
        return NULL;

    float **ptrs[] = {&params->wte,      &params->wpe,      &params->ln1w,
                      &params->ln1b,     &params->qkvw,     &params->qkvb,
                      &params->attprojw, &params->attprojb, &params->ln2w,
                      &params->ln2b,     &params->fcw,      &params->fcb,
                      &params->fcprojw,  &params->fcprojb,  &params->lnfw,
                      &params->lnfb};
    float  *params_memory_iterator = params_memory;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        *(ptrs[i]) = params_memory_iterator;
        params_memory_iterator += param_sizes[i];
    }
    return params_memory;
}

float *gpt2_malloc_and_point_activations(ActivationTensors *acts,
                                         size_t            *act_sizes,
                                         void *(*alloc_fn)(size_t)) {
    size_t num_activations = 0;
    for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
        num_activations += act_sizes[i];
    }
    float *acts_memory = (float *)alloc_fn(num_activations * sizeof(float));
    if (!acts_memory)
        return NULL;

    float **ptrs[] = {
        &acts->encoded,    &acts->ln1,       &acts->ln1_mean, &acts->ln1_rstd,
        &acts->qkv,        &acts->atty,      &acts->preatt,   &acts->att,
        &acts->attproj,    &acts->residual2, &acts->ln2,      &acts->ln2_mean,
        &acts->ln2_rstd,   &acts->fch,       &acts->fch_gelu, &acts->fcproj,
        &acts->residual3,  &acts->lnf,       &acts->lnf_mean, &acts->lnf_rstd,
        &acts->logits,     &acts->probs,     &acts->losses,   &acts->key_cache,
        &acts->value_cache};
    float *acts_memory_iterator = acts_memory;
    for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
        *(ptrs[i]) = acts_memory_iterator;
        acts_memory_iterator += act_sizes[i];
    }
    return acts_memory;
}

size_t gpt2_prepare_param_layout(GPT2 *model, const int *model_header) {
    int maxT = model_header[2];
    int V = model_header[3];
    int L = model_header[4];
    int NH = model_header[5];
    int C = model_header[6];
    model->config.max_seq_len = maxT;
    model->config.vocab_size = V;
    model->config.num_layers = L;
    model->config.num_heads = NH;
    model->config.channels = C;

    model->param_sizes[0] = V * C;
    model->param_sizes[1] = maxT * C;
    model->param_sizes[2] = L * C;
    model->param_sizes[3] = L * C;
    model->param_sizes[4] = L * (3 * C) * C;
    model->param_sizes[5] = L * (3 * C);
    model->param_sizes[6] = L * C * C;
    model->param_sizes[7] = L * C;
    model->param_sizes[8] = L * C;
    model->param_sizes[9] = L * C;
    model->param_sizes[10] = L * (4 * C) * C;
    model->param_sizes[11] = L * (4 * C);
    model->param_sizes[12] = L * C * (4 * C);
    model->param_sizes[13] = L * C;
    model->param_sizes[14] = C;
    model->param_sizes[15] = C;

    size_t num_parameters = 0;
    for (size_t i = 0; i < NUM_PARAMETER_TENSORS; i++) {
        num_parameters += model->param_sizes[i];
    }
    model->num_parameters = num_parameters;
    return num_parameters;
}

void gpt2_build_from_buffer(GPT2          *model,
                            const uint8_t *buffer,
                            size_t         len,
                            void *(*alloc_fn)(size_t),
                            void (*error_fn)(const char *)) {
    if (buffer == NULL) {
        error_fn("checkpoint buffer is null");
        return;
    }
    size_t header_bytes = sizeof(int) * 256;
    if (len < header_bytes) {
        error_fn("checkpoint buffer is too small");
        return;
    }
    int model_header[256];
    memcpy(model_header, buffer, header_bytes);
    if (model_header[0] != 20240326) {
        error_fn("Bad magic model buffer");
        return;
    }
    if (model_header[1] != 1) {
        error_fn("Bad version in model buffer");
        return;
    }
    size_t num_parameters = gpt2_prepare_param_layout(model, model_header);
    size_t params_bytes = num_parameters * sizeof(float);
    if (len < header_bytes + params_bytes) {
        error_fn("checkpoint buffer truncated");
        return;
    }
    model->params_memory = gpt2_malloc_and_point_parameters(
        &model->params, model->param_sizes, alloc_fn);
    if (!model->params_memory) {
        error_fn("parameter allocation failed");
        return;
    }
    memcpy(model->params_memory, buffer + header_bytes, params_bytes);
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
    model->cache_pos = 0;
}

void gpt2_forward(GPT2 *model,
                  int  *inputs,
                  int  *targets,
                  int   B,
                  int   T,
                  void (*error_fn)(const char *)) {
    if (model->params_memory == NULL) {
        error_fn("model was not initialized properly");
        return;
    }

    int V = model->config.vocab_size;
    int L = model->config.num_layers;
    int NH = model->config.num_heads;
    int C = model->config.channels;

    if (model->acts_memory == NULL) {
        model->batch_size = B;
        model->seq_len = T;
        model->act_sizes[0] = B * T * C;
        model->act_sizes[1] = L * B * T * C;
        model->act_sizes[2] = L * B * T;
        model->act_sizes[3] = L * B * T;
        model->act_sizes[4] = L * B * T * 3 * C;
        model->act_sizes[5] = L * B * T * C;
        model->act_sizes[6] = L * B * NH * T * T;
        model->act_sizes[7] = L * B * NH * T * T;
        model->act_sizes[8] = L * B * T * C;
        model->act_sizes[9] = L * B * T * C;
        model->act_sizes[10] = L * B * T * C;
        model->act_sizes[11] = L * B * T;
        model->act_sizes[12] = L * B * T;
        model->act_sizes[13] = L * B * T * 4 * C;
        model->act_sizes[14] = L * B * T * 4 * C;
        model->act_sizes[15] = L * B * T * C;
        model->act_sizes[16] = L * B * T * C;
        model->act_sizes[17] = B * T * C;
        model->act_sizes[18] = B * T;
        model->act_sizes[19] = B * T;
        model->act_sizes[20] = B * T * V;
        model->act_sizes[21] = B * T * V;
        model->act_sizes[22] = B * T;
        model->act_sizes[23] = L * model->config.max_seq_len * C;
        model->act_sizes[24] = L * model->config.max_seq_len * C;
        size_t num_activations = 0;
        for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
            num_activations += model->act_sizes[i];
        }
        model->num_activations = num_activations;
        // Note: For activations, we use malloc directly since this is called
        // from gpt2_forward which doesn't have platform context yet.
        // In practice, this will be refactored to use platform allocator.
        // Note: For activations, we use malloc directly since this is called
        // from gpt2_forward which doesn't have platform context yet.
        // In practice, this will be refactored to use platform allocator.
        model->acts_memory = gpt2_malloc_and_point_activations(
            &model->acts, model->act_sizes, malloc);
        if (!model->acts_memory) {
            error_fn("activation allocation failed");
            return;
        }
        model->inputs = (int *)malloc(B * T * sizeof(int));
        model->targets = (int *)malloc(B * T * sizeof(int));
        if (!model->inputs || !model->targets) {
            error_fn("input/target allocation failed");
            return;
        }
    } else {
        if (B > model->batch_size || T > model->seq_len) {
            error_fn("batch size or sequence length is inadequately large");
            return;
        }
    }

    memcpy(model->inputs, inputs, B * T * sizeof(int));
    if (targets != NULL) {
        memcpy(model->targets, targets, B * T * sizeof(int));
    }

    ParameterTensors  params = model->params;
    ActivationTensors acts = model->acts;
    float            *residual;
    encoder_forward(acts.encoded, inputs, params.wte, params.wpe, B, T, C);
    for (int l = 0; l < L; l++) {
        residual = l == 0 ? acts.encoded : acts.residual3 + (l - 1) * B * T * C;

        float *l_ln1w = params.ln1w + l * C;
        float *l_ln1b = params.ln1b + l * C;
        float *l_qkvw = params.qkvw + l * 3 * C * C;
        float *l_qkvb = params.qkvb + l * 3 * C;
        float *l_attprojw = params.attprojw + l * C * C;
        float *l_attprojb = params.attprojb + l * C;
        float *l_ln2w = params.ln2w + l * C;
        float *l_ln2b = params.ln2b + l * C;
        float *l_fcw = params.fcw + l * 4 * C * C;
        float *l_fcb = params.fcb + l * 4 * C;
        float *l_fcprojw = params.fcprojw + l * C * 4 * C;
        float *l_fcprojb = params.fcprojb + l * C;

        float *l_ln1 = acts.ln1 + l * B * T * C;
        float *l_ln1_mean = acts.ln1_mean + l * B * T;
        float *l_ln1_rstd = acts.ln1_rstd + l * B * T;
        float *l_qkv = acts.qkv + l * B * T * 3 * C;
        float *l_atty = acts.atty + l * B * T * C;
        float *l_preatt = acts.preatt + l * B * NH * T * T;
        float *l_att = acts.att + l * B * NH * T * T;
        float *l_attproj = acts.attproj + l * B * T * C;
        float *l_residual2 = acts.residual2 + l * B * T * C;
        float *l_ln2 = acts.ln2 + l * B * T * C;
        float *l_ln2_mean = acts.ln2_mean + l * B * T;
        float *l_ln2_rstd = acts.ln2_rstd + l * B * T;
        float *l_fch = acts.fch + l * B * T * 4 * C;
        float *l_fch_gelu = acts.fch_gelu + l * B * T * 4 * C;
        float *l_fcproj = acts.fcproj + l * B * T * C;
        float *l_residual3 = acts.residual3 + l * B * T * C;

        layernorm_forward(l_ln1, l_ln1_mean, l_ln1_rstd, residual, l_ln1w,
                          l_ln1b, B, T, C);
        matmul_forward(l_qkv, l_ln1, l_qkvw, l_qkvb, B, T, C, 3 * C);
        attention_forward(l_atty, l_preatt, l_att, l_qkv, B, T, C, NH);
        matmul_forward(l_attproj, l_atty, l_attprojw, l_attprojb, B, T, C, C);
        residual_forward(l_residual2, residual, l_attproj, B * T * C);
        layernorm_forward(l_ln2, l_ln2_mean, l_ln2_rstd, l_residual2, l_ln2w,
                          l_ln2b, B, T, C);
        matmul_forward(l_fch, l_ln2, l_fcw, l_fcb, B, T, C, 4 * C);
        gelu_forward(l_fch_gelu, l_fch, B * T * 4 * C);
        matmul_forward(l_fcproj, l_fch_gelu, l_fcprojw, l_fcprojb, B, T, 4 * C,
                       C);
        residual_forward(l_residual3, l_residual2, l_fcproj, B * T * C);
    }
    residual = acts.residual3 + (L - 1) * B * T * C;
    layernorm_forward(acts.lnf, acts.lnf_mean, acts.lnf_rstd, residual,
                      params.lnfw, params.lnfb, B, T, C);
    matmul_forward(acts.logits, acts.lnf, params.wte, NULL, B, T, C, V);
    softmax_forward(acts.probs, acts.logits, B, T, V);

    if (targets != NULL) {
        crossentropy_forward(model->acts.losses, model->acts.probs, targets, B,
                             T, V);
        float mean_loss = 0.0f;
        for (int i = 0; i < B * T; i++) {
            mean_loss += model->acts.losses[i];
        }
        mean_loss /= B * T;
        model->mean_loss = mean_loss;
    } else {
        model->mean_loss = -1.0f;
    }
}

void gpt2_forward_single(GPT2 *model,
                         int   token,
                         int   pos,
                         void (*error_fn)(const char *)) {
    if (model->params_memory == NULL) {
        error_fn("model was not initialized properly");
        return;
    }

    int V = model->config.vocab_size;
    int L = model->config.num_layers;
    int NH = model->config.num_heads;
    int C = model->config.channels;
    int T_cache =
        model->seq_len > 0 ? model->seq_len : model->config.max_seq_len;
    if (T_cache <= 0) {
        error_fn("invalid max sequence length");
        return;
    }
    if (pos < 0 || pos >= T_cache) {
        error_fn("cache position out of range");
        return;
    }
    if (token < 0 || token >= V) {
        error_fn("token id out of range");
        return;
    }

    if (model->acts_memory == NULL) {
        int B = 1;
        int T = T_cache;
        model->batch_size = B;
        model->seq_len = T;
        model->act_sizes[0] = B * T * C;
        model->act_sizes[1] = L * B * T * C;
        model->act_sizes[2] = L * B * T;
        model->act_sizes[3] = L * B * T;
        model->act_sizes[4] = L * B * T * 3 * C;
        model->act_sizes[5] = L * B * T * C;
        model->act_sizes[6] = L * B * NH * T * T;
        model->act_sizes[7] = L * B * NH * T * T;
        model->act_sizes[8] = L * B * T * C;
        model->act_sizes[9] = L * B * T * C;
        model->act_sizes[10] = L * B * T * C;
        model->act_sizes[11] = L * B * T;
        model->act_sizes[12] = L * B * T;
        model->act_sizes[13] = L * B * T * 4 * C;
        model->act_sizes[14] = L * B * T * 4 * C;
        model->act_sizes[15] = L * B * T * C;
        model->act_sizes[16] = L * B * T * C;
        model->act_sizes[17] = B * T * C;
        model->act_sizes[18] = B * T;
        model->act_sizes[19] = B * T;
        model->act_sizes[20] = B * T * V;
        model->act_sizes[21] = B * T * V;
        model->act_sizes[22] = B * T;
        model->act_sizes[23] = L * model->config.max_seq_len * C;
        model->act_sizes[24] = L * model->config.max_seq_len * C;
        size_t num_activations = 0;
        for (size_t i = 0; i < NUM_ACTIVATION_TENSORS; i++) {
            num_activations += model->act_sizes[i];
        }
        model->num_activations = num_activations;
        model->acts_memory = gpt2_malloc_and_point_activations(
            &model->acts, model->act_sizes, malloc);
        if (!model->acts_memory) {
            error_fn("activation allocation failed");
            return;
        }
        model->inputs = (int *)malloc(B * T * sizeof(int));
        model->targets = (int *)malloc(B * T * sizeof(int));
        if (!model->inputs || !model->targets) {
            error_fn("input/target allocation failed");
            return;
        }
    }

    ParameterTensors  params = model->params;
    ActivationTensors acts = model->acts;

    float *encoded_pos = acts.encoded + pos * C;
    float *wte_ix = params.wte + token * C;
    float *wpe_pos = params.wpe + pos * C;
    for (int i = 0; i < C; i++) {
        encoded_pos[i] = wte_ix[i] + wpe_pos[i];
    }

    int   hs = C / NH;
    float scale = 1.0f / sqrtf((float)hs);
    for (int l = 0; l < L; l++) {
        float *residual =
            l == 0 ? encoded_pos
                   : acts.residual3 + (l - 1) * T_cache * C + pos * C;

        float *l_ln1w = params.ln1w + l * C;
        float *l_ln1b = params.ln1b + l * C;
        float *l_qkvw = params.qkvw + l * 3 * C * C;
        float *l_qkvb = params.qkvb + l * 3 * C;
        float *l_attprojw = params.attprojw + l * C * C;
        float *l_attprojb = params.attprojb + l * C;
        float *l_ln2w = params.ln2w + l * C;
        float *l_ln2b = params.ln2b + l * C;
        float *l_fcw = params.fcw + l * 4 * C * C;
        float *l_fcb = params.fcb + l * 4 * C;
        float *l_fcprojw = params.fcprojw + l * C * 4 * C;
        float *l_fcprojb = params.fcprojb + l * C;

        float *l_ln1 = acts.ln1 + l * T_cache * C + pos * C;
        float *l_ln1_mean = acts.ln1_mean + l * T_cache + pos;
        float *l_ln1_rstd = acts.ln1_rstd + l * T_cache + pos;
        float *l_qkv = acts.qkv + l * T_cache * 3 * C + pos * 3 * C;
        float *l_atty = acts.atty + l * T_cache * C + pos * C;
        float *l_preatt = acts.preatt + l * NH * T_cache * T_cache;
        float *l_att = acts.att + l * NH * T_cache * T_cache;
        float *l_attproj = acts.attproj + l * T_cache * C + pos * C;
        float *l_residual2 = acts.residual2 + l * T_cache * C + pos * C;
        float *l_ln2 = acts.ln2 + l * T_cache * C + pos * C;
        float *l_ln2_mean = acts.ln2_mean + l * T_cache + pos;
        float *l_ln2_rstd = acts.ln2_rstd + l * T_cache + pos;
        float *l_fch = acts.fch + l * T_cache * 4 * C + pos * 4 * C;
        float *l_fch_gelu = acts.fch_gelu + l * T_cache * 4 * C + pos * 4 * C;
        float *l_fcproj = acts.fcproj + l * T_cache * C + pos * C;
        float *l_residual3 = acts.residual3 + l * T_cache * C + pos * C;

        layernorm_forward(l_ln1, l_ln1_mean, l_ln1_rstd, residual, l_ln1w,
                          l_ln1b, 1, 1, C);
        matmul_forward(l_qkv, l_ln1, l_qkvw, l_qkvb, 1, 1, C, 3 * C);

        float *k_cache_pos = acts.key_cache + l * T_cache * C + pos * C;
        float *v_cache_pos = acts.value_cache + l * T_cache * C + pos * C;
        memcpy(k_cache_pos, l_qkv + C, C * sizeof(float));
        memcpy(v_cache_pos, l_qkv + 2 * C, C * sizeof(float));

        for (int h = 0; h < NH; h++) {
            float *query = l_qkv + h * hs;
            float *preatt_row =
                l_preatt + h * T_cache * T_cache + pos * T_cache;
            float *att_row = l_att + h * T_cache * T_cache + pos * T_cache;

            float maxval = -10000.0f;
            for (int t2 = 0; t2 <= pos; t2++) {
                float *key_t2 =
                    acts.key_cache + l * T_cache * C + t2 * C + h * hs;
                float val = 0.0f;
                for (int i = 0; i < hs; i++) {
                    val += query[i] * key_t2[i];
                }
                val *= scale;
                if (val > maxval) {
                    maxval = val;
                }
                preatt_row[t2] = val;
            }

            float expsum = 0.0f;
            for (int t2 = 0; t2 <= pos; t2++) {
                float expv = expf(preatt_row[t2] - maxval);
                expsum += expv;
                att_row[t2] = expv;
            }
            float expsum_inv = expsum == 0.0f ? 0.0f : 1.0f / expsum;
            for (int t2 = 0; t2 <= pos; t2++) {
                att_row[t2] *= expsum_inv;
            }

            float *out_bth = l_atty + h * hs;
            for (int i = 0; i < hs; i++) {
                out_bth[i] = 0.0f;
            }
            for (int t2 = 0; t2 <= pos; t2++) {
                float *value_t2 =
                    acts.value_cache + l * T_cache * C + t2 * C + h * hs;
                float att_t2 = att_row[t2];
                for (int i = 0; i < hs; i++) {
                    out_bth[i] += att_t2 * value_t2[i];
                }
            }
        }

        matmul_forward(l_attproj, l_atty, l_attprojw, l_attprojb, 1, 1, C, C);
        residual_forward(l_residual2, residual, l_attproj, C);
        layernorm_forward(l_ln2, l_ln2_mean, l_ln2_rstd, l_residual2, l_ln2w,
                          l_ln2b, 1, 1, C);
        matmul_forward(l_fch, l_ln2, l_fcw, l_fcb, 1, 1, C, 4 * C);
        gelu_forward(l_fch_gelu, l_fch, 4 * C);
        matmul_forward(l_fcproj, l_fch_gelu, l_fcprojw, l_fcprojb, 1, 1, 4 * C,
                       C);
        residual_forward(l_residual3, l_residual2, l_fcproj, C);
    }

    float *residual = acts.residual3 + (L - 1) * T_cache * C + pos * C;
    float *lnf_pos = acts.lnf + pos * C;
    float *lnf_mean_pos = acts.lnf_mean + pos;
    float *lnf_rstd_pos = acts.lnf_rstd + pos;
    layernorm_forward(lnf_pos, lnf_mean_pos, lnf_rstd_pos, residual,
                      params.lnfw, params.lnfb, 1, 1, C);
    matmul_forward(acts.logits + pos * V, lnf_pos, params.wte, NULL, 1, 1, C,
                   V);

    model->mean_loss = -1.0f;
}

void gpt2_free(GPT2 *model, void (*free_fn)(void *)) {
    if (model->params_memory) {
        free_fn(model->params_memory);
    }
    if (model->grads_memory) {
        free_fn(model->grads_memory);
    }
    if (model->m_memory) {
        free_fn(model->m_memory);
    }
    if (model->v_memory) {
        free_fn(model->v_memory);
    }
    if (model->acts_memory) {
        free_fn(model->acts_memory);
    }
    if (model->grads_acts_memory) {
        free_fn(model->grads_acts_memory);
    }
    if (model->inputs) {
        free_fn(model->inputs);
    }
    if (model->targets) {
        free_fn(model->targets);
    }
}
