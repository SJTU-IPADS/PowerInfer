#include <cstddef>
#include <cstdio>

#include "powerinfer-cpu-data.hpp"
#include "powerinfer-perf.hpp"
#include "convert.hpp"
#include "sparse_moe_ffn.hpp"
#include "powerinfer-cpu-param.hpp"
#include "powerinfer-cpu.hpp"

namespace detail {

    template<int T_n_ff, int T_n_embd>
    PowerInferError powerinfer_host_ffn_moe_sparse_q4_0_f32_inner(const HostComputeParam param,
            const void *up_data, const void *gate_data, const void *down_data, const int *router_data, const float *input_data, float *dst_data,
            const int batch_size, const int router_nb1) {

        const int ith               = param.ith;
        const int nth               = param.nth;
        void *    wdata             = param.wdata;
        const size_t wsize          = param.wsize;
    
        // | -- up & gate output -- |
        // We assume that the dst data is larger than dequantized input because they share the same shape while the latter is quantized.
        block_q8_0 * dequantized_input           = reinterpret_cast<block_q8_0 *>(dst_data);
        block_q8_0 * up_gate_output              = static_cast<block_q8_0 *>(wdata);
        constexpr size_t up_gate_output_row_size = powerinfer_row_size(POWERINFER_TYPE_Q8_0, T_n_ff * SPARSE_MOE_NUM_HEAD);
        const size_t up_gate_output_size         = batch_size * up_gate_output_row_size;

        const size_t atomic_counter_align        = 64;
        const size_t atomic_counter_size         = atomic_counter_align + 2 * sizeof(std::atomic_int);
        const size_t atomic_counter_align_offset = (up_gate_output_size + atomic_counter_align - 1) / atomic_counter_align * atomic_counter_align;
        std::atomic_int *atomic_counter_ptr      = reinterpret_cast<std::atomic_int *>(static_cast<char *>(wdata) + atomic_counter_align_offset);
        if (ith == 0) {
            new (atomic_counter_ptr) std::atomic_int[2] {0, 0};
        }
     
        if (up_gate_output_size + atomic_counter_size > wsize) {
            return { true, "The compute buffer is too small" };
        }
    
        // dequantize
        {
            constexpr int GROUP_Q8_NUM  = 4;
            constexpr int GROUP_F32_NUM = QK8_0 * GROUP_Q8_NUM;
            constexpr int NUM_GROUP     = T_n_embd / GROUP_F32_NUM;
            static_assert(T_n_embd % GROUP_F32_NUM == 0);
            const     int num_task      = batch_size * NUM_GROUP;

            for (int task_id = ith; task_id < num_task; task_id += nth) {
                const float *cur_src_group = input_data + task_id * GROUP_F32_NUM;
                block_q8_0  *cur_dst_group = dequantized_input + task_id * GROUP_Q8_NUM;
                quantize_row_q8_0(cur_src_group, cur_dst_group, GROUP_F32_NUM);                      
            }
        }
        param.arrive_and_wait();
    
        // calculate up & gate
        powerinfer_begin_event("up & gate");
        ggml_compute_forward_mul_mat_up_gate_moe_sparse<T_n_ff, T_n_embd>(nth, atomic_counter_ptr[0], 
            up_data, gate_data,
            dequantized_input, router_data, up_gate_output, 
            batch_size, router_nb1
        ); 
        powerinfer_end_event();
        param.arrive_and_wait();
        
        // calculate down
        powerinfer_begin_event("down");
        ggml_compute_forward_mul_mat_moe_sparse<T_n_ff, T_n_embd>(nth, atomic_counter_ptr[1], 
            down_data, up_gate_output, router_data, dst_data,
            batch_size, router_nb1
        );
        powerinfer_end_event();
    
        return { false, "Success" };
    }
}

PowerInferError powerinfer_host_ffn_moe_sparse_q4_0_f32_impl(const HostComputeParam param,
        const float *ffn_norm_ptr,
        const void *up_ptr, const void *gate_ptr, const void *down_ptr, const void *router_ptr,
        const float *last_attn_norm_ptr, const float *output_norm_ptr,
        const float *inpSA_ptr, const float *input_ptr,
        float *dst_ptr,
        const int up_ncols, const int up_nrows, const int batch_size,
        const float rms_norm_eps) {

    // // | -- norm buffer -- | -- residual buffer -- |
    // void *buffer_ptr = param.wdata;
    
    // float *ffn_norm_buffer     = nullptr;
    // float *ffn_inp_residual_buffer = nullptr;

    // float *out_norm_ptr     = dst_ptr;
    // float *out_residual_ptr = dst_ptr + up_ncols * up_nrows;

    // powerinfer_host_post_attn_layernorm_impl(param, last_attn_norm_ptr == nullptr ? nullptr : inpSA_ptr, input_ptr, ffn_norm_ptr, nullptr, 
    //     ffn_norm_buffer, ffn_inp_residual_buffer, up_ncols, up_nrows, rms_norm_eps);
    
    // param.arrive_and_wait();

    // if (n_ff == 896 && n_embd == 4096) {
    //     return detail::powerinfer_host_ffn_moe_sparse_q4_0_f32_inner<896, 4096>(param,
    //         up_ptr, gate_ptr, down_ptr, router_ptr, input_data, dst_data,
    //         batch_size, router_nb1
    //     );
    // } else {
    //     return { true, "Unsupported n_ff or n_embd" };
    // }

    // param.arrive_and_wait();

    // if (last_attn_norm_ptr != nullptr) {
    //     powerinfer_host_post_attn_layernorm_impl(param, ffn_inp_residual_buffer, ffn_out_ptr, output_norm_ptr, nullptr, 
    //         out_norm_ptr, out_residual_ptr, up_ncols, up_nrows, rms_norm_eps);
    // }

    return { "true", "unimplement" };
}
