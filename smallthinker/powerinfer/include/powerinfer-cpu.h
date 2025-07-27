#pragma once

#include <stddef.h>
#include <stdint.h>

#include "powerinfer-api.h"
#include "powerinfer-error.h"

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

#ifndef POWERINFER_CPU_PARAM_DEF
#define POWERINFER_CPU_PARAM_DEF

    struct PowerInferCPUParam {
        int     loader_id;
        int     ith;
        int     nth;
        void *  wdata;
        size_t  wsize;
    };

#endif // POWERINFER_CPU_PARAM_DEF

#define SIGMOID_ROUTER_BLOCK_SIZE 16

    /*
     * Initialization
     */

    /*!
     * @brief Initialize the pointer to the fp16 table
     * @param[in] table_ptr The pointer to the fp16 table
     * @note This is only needed for once
     */
    POWERINFER_API void powerinfer_init_f16_table(const float *table_ptr);

    /*
     * Operators
     */

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
    POWERINFER_API struct PowerInferError powerinfer_host_ffn_q4_0_f32(struct PowerInferCPUParam param,
                                            const float *ffn_norm_ptr,
                                            const void *up_ptr, const void *gate_ptr, const void *down_ptr, const void *router_ptr,
                                            const float *last_attn_norm_ptr, const float *output_norm_ptr,
                                            const float *inpSA_ptr, const float *input_ptr,
                                            float *dst_ptr,
                                            int up_ncols, int up_nrows, int batch_size,
                                            float rms_norm_eps);

    POWERINFER_API struct PowerInferError powerinfer_host_fused_sparse_ffn(
        struct PowerInferCPUParam param,
        size_t batch_size,
        size_t embed_dim,
        size_t ffn_hidden_dim,
        const void *up_weight,
        const void *gate_weight,
        const void *down_weight,
        const float *activation,
        const float *router_out,
        float *output
    );

    POWERINFER_API struct PowerInferError powerinfer_host_fused_sparse_moe(
        struct PowerInferCPUParam param,
        size_t batch_size,
        size_t embed_dim,
        size_t ffn_hidden_dim,
        size_t n_expert_used,
        const void *up_weight,
        const void *gate_weight,
        const void *down_weight,
        const float *activation,
        const int32_t *selected_experts,
        const float *expert_weights,
        float *output,
        size_t qk=32
    );


    POWERINFER_API void powerinfer_init_global_expert_cache(
        const char *expert_bundle_path,
        size_t n_layers,
        size_t n_experts,
        size_t n_matrices,
        size_t matrix_bytes
    );

    POWERINFER_API bool powerinfer_has_global_expert_cache(void);

    POWERINFER_API void powerinfer_init_moe_pipeline(
        size_t n_workers,
        size_t n_layers,
        size_t embed_dim,
        size_t ffn_hidden_dim,
        size_t batch_size,
        size_t n_experts,
        size_t n_used_experts,
        bool normalize_scores
    );

    POWERINFER_API void powerinfer_moe_pipeline_prefetch(
        size_t layer_id,
        size_t batch_size,
        size_t n_predicted_experts,
        size_t max_n_prefetch,
        const int32_t *expert_ids
    );

    POWERINFER_API void powerinfer_moe_pipeline_build_tasks(
        size_t layer_id,
        size_t batch_size,
        int ffn_op_type,  // An ugly impl, 0 for RELU, 1 for SiLU
        const int32_t *expert_ids
    );

    POWERINFER_API struct PowerInferError powerinfer_host_moe_pipeline_forward(
        struct PowerInferCPUParam param,
        size_t layer_id,
        const float *expert_logits,  // Shape: [batch_size, n_experts]
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
    POWERINFER_API struct PowerInferError powerinfer_host_lmhead_q4_0_f32(struct PowerInferCPUParam param,
        const void *profiler_data, const void *lmhead_data, const float *input_data, float *dst_data,
        int num_embd, int num_vocab, int profiler_num_element);


    POWERINFER_API struct PowerInferError powerinfer_host_rotary_embedding_fp32(struct PowerInferCPUParam param,
        const float *key_ptr, const float *query_ptr, const int *position_ptr,
        float *dst_ptr, int mode,
        int num_token, int num_head_q, int num_head_kv,
        int query_row_size, int key_row_size);

    POWERINFER_API struct PowerInferError powerinfer_host_post_attn_layernorm(
        const struct PowerInferCPUParam param,
        const float *inpSA_data, const float *attn_out_data, const float *weight_data, const float *bias_data,
        float *dst_data, float *residual_data,
        int ne00, int nrows, float eps);

#ifdef __cplusplus
    }
#endif // __cplusplus
