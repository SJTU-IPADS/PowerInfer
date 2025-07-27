#include <cstddef>
#include <cstdio>

#include "powerinfer-cpu-data.hpp"
#include "powerinfer-perf.hpp"
#include "convert.hpp"
#include "sparse_moe_ffn.hpp"
#include "powerinfer-cpu-param.hpp"
#include "powerinfer-cpu.hpp"
#include "powerinfer_cond_ffn.hpp"

PowerInferError powerinfer_host_ffn_cond_q4_0_f32_impl(const HostComputeParam param,
        const float *ffn_norm_ptr,
        const void *up_ptr, const void *gate_ptr, const void *down_ptr,
        const float *last_attn_norm_ptr, const float *output_norm_ptr,
        const float *inpSA_ptr, const float *input_ptr,
        float *dst_ptr,
        const int up_ncols, const int up_nrows, const int batch_size,
        const float rms_norm_eps) {
    
    const int hidden_size       = up_ncols;
    const int intermediate_size = up_nrows;

    // | -- norm buffer -- | -- residual buffer -- | -- ffn up gate buffer -- |
    
    float *buffer_ptr = static_cast<float *>(param.wdata);

    float *ffn_norm_buffer          = buffer_ptr;
    float *ffn_inp_residual_buffer  = buffer_ptr + hidden_size * batch_size;
    float *ffn_out_buffer           = buffer_ptr;

    float *out_norm_ptr     = dst_ptr;
    float *out_residual_ptr = dst_ptr + hidden_size * batch_size;

    const size_t norm_buffer_size = hidden_size * batch_size * 2 * sizeof(float);

    const size_t atomic_counter_align        = 64;
    const size_t atomic_counter_size         = atomic_counter_align + 2 * sizeof(std::atomic_int);
    const size_t atomic_counter_align_offset = (norm_buffer_size + atomic_counter_align - 1) / atomic_counter_align * atomic_counter_align;
    std::atomic_int *atomic_counter_ptr      = reinterpret_cast<std::atomic_int *>(static_cast<char *>(param.wdata) + atomic_counter_align_offset);

    block_q8_0 * dequantized_input           = reinterpret_cast<block_q8_0 *>(dst_ptr);
    block_q8_0 * up_gate_output              = reinterpret_cast<block_q8_0 *>(static_cast<uint8_t *>(param.wdata) + norm_buffer_size + atomic_counter_size);
    const size_t up_gate_output_row_size     = powerinfer_row_size(POWERINFER_TYPE_Q8_0, intermediate_size * 2);
    const size_t up_gate_output_size         = batch_size * up_gate_output_row_size;

    if (param.wsize < norm_buffer_size + atomic_counter_size + up_gate_output_size) { return { true, "The compute buffer is too small" }; }

    powerinfer_host_post_attn_layernorm_impl(param, last_attn_norm_ptr == nullptr ? nullptr : inpSA_ptr, input_ptr, ffn_norm_ptr, nullptr, 
        ffn_norm_buffer, ffn_inp_residual_buffer, hidden_size, batch_size, rms_norm_eps);
    
    param.arrive_and_wait();

    { // FFN foward

        if (param.ith == 0) { new (atomic_counter_ptr) std::atomic_int[2] {0, 0}; }
     
        // dequantize
        {
            const int GROUP_Q8_NUM  = 4;
            const int GROUP_F32_NUM = QK8_0 * GROUP_Q8_NUM;
            const int NUM_GROUP     = hidden_size / GROUP_F32_NUM;
            POWERINFER_ASSERT(hidden_size % GROUP_F32_NUM == 0);
            const     int num_task      = batch_size * NUM_GROUP;

            for (int task_id = param.ith; task_id < num_task; task_id += param.nth) {
                const float *cur_src_group = ffn_norm_buffer   + task_id * GROUP_F32_NUM;
                block_q8_0  *cur_dst_group = dequantized_input + task_id * GROUP_Q8_NUM;
                quantize_row_q8_0(cur_src_group, cur_dst_group, GROUP_F32_NUM);                      
            }
        }
        param.arrive_and_wait();
    
        // calculate up & gate
        powerinfer_begin_event("up & gate");
        ggml_compute_forward_mul_mat_up_gate(param.nth, atomic_counter_ptr[0], 
            up_ptr, gate_ptr,
            dequantized_input, up_gate_output, 
            intermediate_size, hidden_size, batch_size
        ); 
        powerinfer_end_event();
        param.arrive_and_wait();
        
        // calculate down
        powerinfer_begin_event("down");
        ggml_compute_forward_mul_mat_down(param.nth, atomic_counter_ptr[1], 
            down_ptr, up_gate_output, ffn_out_buffer,
            intermediate_size, hidden_size, batch_size
        );
        powerinfer_end_event();
    }

    param.arrive_and_wait();

    if (last_attn_norm_ptr != nullptr) {
        powerinfer_host_post_attn_layernorm_impl(param, ffn_inp_residual_buffer, ffn_out_buffer, output_norm_ptr, nullptr, 
            out_norm_ptr, out_residual_ptr, up_ncols, batch_size, rms_norm_eps);
    }

    return { false, "Success" };
}
