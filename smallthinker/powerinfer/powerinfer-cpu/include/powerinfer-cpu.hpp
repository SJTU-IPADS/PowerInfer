#pragma once

#include <cstddef>
#include "powerinfer-macro.hpp"
#include "powerinfer-exception.hpp"
#include "powerinfer-cpu-param.hpp"
#include "powerinfer-type.hpp"

/*!
 * @brief Initialize the pointer to the fp16 table
 * @param[in] table_ptr The pointer to the fp16 table
 * @note This is only needed for once
 */
HOST_API void powerinfer_init_f16_table_impl(const float *table_ptr);

/*!
 * @brief Operator: sparse FFN operator for Q4_0 weight and FP32 vector input
 * @param[in]  ith:         The id of the current thread
 * @param[in]  nth:         The number of total thread
 * @param[out] wdata:       The compute buffer for temporary data
 * @param[in]  wsize:       The size of the compute buffer
 * @param[in]  init:        Whether this call comes from init phase
 * @param[in]  up_data:     Up weight of Q4_0
 * @param[in]  gate_data:   Gate weight of Q4_0
 * @param[in]  down_data:   Down weight of Q4_0
 * @param[in]  router_data: The output of predictor
 * @param[in]  input_data:  The input activation of FP32
 * @param[out] dst_data:    The output activation of FP32
 * @param[in]  n_ff:        The intermediate_size (896 for llama3.1-8B)
 * @param[in]  n_embd:      The hidden size (4096 for llama3.1-8B)
 * @param[in]  num_chunk:   The number of chunk (Normally 16 for in memory method)
 * @param[in]  batch_size:  The batch size (Normally input->ne[1])
 * @param[in]  router_nb1:  The size of a row of router
 */
HOST_API PowerInferError powerinfer_host_ffn_moe_sparse_q4_0_f32_impl(HostComputeParam param,
                                            const float *ffn_norm_ptr,
                                            const void *up_ptr, const void *gate_ptr, const void *down_ptr, const void *router_ptr,
                                            const float *last_attn_norm_ptr, const float *output_norm_ptr,
                                            const float *inpSA_ptr, const float *input_ptr,
                                            float *dst_ptr,
                                            int up_ncols, int up_nrows, int batch_size,
                                            float rms_norm_eps);

HOST_API PowerInferError powerinfer_host_ffn_cond_q4_0_f32_impl(HostComputeParam param,
                                            const float *ffn_norm_ptr,
                                            const void *up_ptr, const void *gate_ptr, const void *down_ptr,
                                            const float *last_attn_norm_ptr, const float *output_norm_ptr,
                                            const float *inpSA_ptr, const float *input_ptr,
                                            float *dst_ptr,
                                            int up_ncols, int up_nrows, int batch_size,
                                            float rms_norm_eps);

HOST_API PowerInferError powerinfer_host_fused_sparse_ffn_impl(
    HostComputeParam param,
    size_t batch_size,
    size_t embed_dim,
    size_t ffn_hidden_dim,
    const char *up_weight,
    const char *gate_weight,
    const char *down_weight,
    const float *activation,
    const float *router_out,
    float *output,
    char *wdata
);

HOST_API PowerInferError powerinfer_host_fused_sparse_moe_impl(
    HostComputeParam param,
    size_t batch_size,
    size_t embed_dim,
    size_t ffn_hidden_dim,
    size_t n_expert_used,
    const char *up_weight,
    const char *gate_weight,
    const char *down_weight,
    const float *activation,
    const int32_t *selected_experts,
    const float* expert_weights,
    float *output,
    char *wdata
);


HOST_API void powerinfer_moe_pipeline_forward(
    HostComputeParam &param,
    size_t layer_id,
    const float *expert_logits,
    const float *input,
    float *output
);

/*!
 * @brief Operator: LMHead and Profiler finalization
 * @param[in]  loader_id:       The id of cache corresponding to the model
 * @param[in]  ith:             The id of the current threads
 * @param[in]  nth:             The total number of threads
 * @param[out] wdata:           The compute buffer for temporary data
 * @param[in]  wsize:           The size of the compute buffer
 * @param[in]  init:            Whether this call comes from init phase
 * @param[in]  profiler_output: The output of the profiler
 * @param[in]  lmhead_data:     LMHead weight of Q4_0
 * @param[in]  input_data:      The input activation of FP32
 * @param[out] dst_data:        The output logits of FP32
 * @param[in]  n_embd:          The number of columns in lmhead (Normally 4096 in llama3.1-8B)
 * @param[in]  num_vocab:       The number of rows in lmhead
 * @onte The batch size should be 1
 */
HOST_API PowerInferError powerinfer_host_lmhead_q4_0_f32_impl(HostComputeParam param,
    const void *profiler_data, const void *lmhead_data, const float *input_data, float *dst_data,
    int n_embd, int num_vocab, int profiler_num_element);

HOST_API PowerInferError powerinfer_host_rotary_embedding_fp32_impl(const HostComputeParam &param, const PositionEmbeddingMetadata &meta, const float *rope_cache_ptr,
        const float *key_ptr, const float *query_ptr, const int *position_ptr,
        float *dst_ptr, int mode, int num_token, int num_head_q, int num_head_kv,
        int query_row_size, int key_row_size);

HOST_API PowerInferError powerinfer_host_post_attn_layernorm_impl(HostComputeParam param,
    const float *inpSA_data, const float *attn_out_data, const float *weight_data, const float *bias_data,
    float *dst_data, float *residual_data,
    int ne00, int nrows, float eps);
