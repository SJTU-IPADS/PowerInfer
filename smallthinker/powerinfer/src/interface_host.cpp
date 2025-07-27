/*
 * Host Interface
 */

#include "powerinfer-cpu.hpp"
#include "powerinfer-cpu.h"

std::barrier<>   hack_global_barrier{ 256 };

void powerinfer_init_f16_table(const float *table_ptr) {
    powerinfer_init_f16_table_impl(table_ptr);
}

PowerInferError powerinfer_host_lmhead_q4_0_f32(const struct PowerInferCPUParam param,
        const void *profiler_data, const void *lmhead_data, const float *input_data, float *dst_data,
        const int num_embd, const int num_vocab, const int profiler_num_element) {

    HostComputeParam compute_param={
            .ith           = param.ith,
            .nth           = param.nth,
            .wdata         = param.wdata,
            .wsize         = param.wsize,
            .barrier       = std::addressof(hack_global_barrier),
            .barrier_count = (param.ith == 0) ? 256 - param.nth + 1 : 1
        };


    return powerinfer_host_lmhead_q4_0_f32_impl(compute_param,
        profiler_data, lmhead_data, input_data, dst_data,
        num_embd, num_vocab, profiler_num_element);
}



PowerInferError powerinfer_host_moe_pipeline_forward(
    struct PowerInferCPUParam param,
    size_t layer_id,
    const float *expert_logits,  // Shape: [batch_size, n_experts]
    const float *input,
    float *output
) {
    // const auto &loader = get_powerinfer_loader(param.loader_id);
    HostComputeParam compute_param = {
        .ith           = param.ith,
        .nth           = param.nth,
        .wdata         = param.wdata,
        .wsize         = param.wsize,
        .barrier       = nullptr,
        .barrier_count = 0
    };

    powerinfer_moe_pipeline_forward(
        compute_param,
        layer_id,
        expert_logits,
        input,
        output
    );

    return { false, "Success" };
}


PowerInferError powerinfer_host_fused_sparse_ffn(
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
) {
    HostComputeParam compute_param={
                .ith           = param.ith,
                .nth           = param.nth,
                .wdata         = param.wdata,
                .wsize         = param.wsize,
                .barrier       = nullptr,
                .barrier_count = (param.ith == 0) ? 256 - param.nth + 1 : 1
            };
    

    return powerinfer_host_fused_sparse_ffn_impl(
        compute_param,
        batch_size,
        embed_dim,
        ffn_hidden_dim,
        static_cast<const char *>(up_weight),
        static_cast<const char *>(gate_weight),
        static_cast<const char *>(down_weight),
        activation,
        router_out,
        output,
        (char *)param.wdata
    );
}


PowerInferError powerinfer_host_fused_sparse_moe(
        struct PowerInferCPUParam param,
        size_t batch_size,
        size_t embed_dim,
        size_t ffn_hidden_dim,
        size_t n_expert_used,
        const void *up_weight,
        const void *gate_weight,
        const void *down_weight,
        const float *activation,
        const int32_t* selected_experts,
        const float *expert_weights,
        float *output,
        size_t qk
) {
    HostComputeParam compute_param={
                .ith           = param.ith,
                .nth           = param.nth,
                .wdata         = param.wdata,
                .wsize         = param.wsize,
                .barrier       = nullptr,
                .barrier_count = (param.ith == 0) ? 256 - param.nth + 1 : 1
            };
    

    return powerinfer_host_fused_sparse_moe_impl(
        compute_param,
        batch_size,
        embed_dim,
        ffn_hidden_dim,
        n_expert_used,
        static_cast<const char *>(up_weight),
        static_cast<const char *>(gate_weight),
        static_cast<const char *>(down_weight),
        activation,
        selected_experts,
        expert_weights,
        output,
        (char *)param.wdata
    );
}


PowerInferError powerinfer_host_fused_sparse_moe(
        struct PowerInferCPUParam param,
        size_t batch_size,
        size_t embed_dim,
        size_t ffn_hidden_dim,
        size_t n_expert_used,
        const void *up_weight,
        const void *gate_weight,
        const void *down_weight,
        const float *activation,
        const int32_t* selected_experts,
        const float *expert_weights,
        float *output
) {
    HostComputeParam compute_param={
                .ith           = param.ith,
                .nth           = param.nth,
                .wdata         = param.wdata,
                .wsize         = param.wsize,
                .barrier       = nullptr,
                .barrier_count = (param.ith == 0) ? 256 - param.nth + 1 : 1
            };
   

    return powerinfer_host_fused_sparse_moe_impl(
        compute_param,
        batch_size,
        embed_dim,
        ffn_hidden_dim,
        n_expert_used,
        static_cast<const char *>(up_weight),
        static_cast<const char *>(gate_weight),
        static_cast<const char *>(down_weight),
        activation,
        selected_experts,
        expert_weights,
        output,
        (char *)param.wdata
    );
}
