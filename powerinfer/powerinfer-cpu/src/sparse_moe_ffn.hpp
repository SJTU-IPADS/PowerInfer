#pragma once

#include <algorithm>
#include <atomic>

#include "convert.hpp"
#include "powerinfer-cpu-data.hpp"
#include "vec_dot.hpp"

template<int T_n_ff, int T_n_embd>
inline static void ggml_compute_forward_up_gate_moe_sparse_one_chunk(
    const void *        __restrict__ up_data, 
    const void *        __restrict__ gate_data,
    const block_q8_0 *  __restrict__ wdata,
    const int *         __restrict__ router_data,
    block_q8_0 *        __restrict__ dst_ptr,
    const int router_row_size,
    const int ir0_start,
    const int ir0_end,
    const int ir1_start,
    const int ir1_end) {

    constexpr int nb1       = powerinfer_row_size(POWERINFER_TYPE_Q8_0, T_n_ff * SPARSE_MOE_NUM_HEAD);
    constexpr int nb01      = powerinfer_row_size(POWERINFER_TYPE_Q4_0, T_n_embd);
    constexpr int row_size  = powerinfer_row_size(POWERINFER_TYPE_Q8_0, T_n_embd);

    // block-tiling attempt
    constexpr int blck_0 = QK8_0;
    constexpr int blck_1 = 16;

    constexpr vec_dot_func_t vec_dot = ggml_vec_dot_q4_0_q8_0;

    // attempt to reduce false-sharing (does not seem to make a difference)
    // 16 * 2, accounting for mmla kernels
    for (int iir1 = ir1_start; iir1 < ir1_end; iir1 += blck_1) {
        for (int iir0 = ir0_start; iir0 < ir0_end; iir0 += blck_0) {
            for (int ir1 = iir1; ir1 < iir1 + blck_1 && ir1 < ir1_end; ir1++) {
                const int i11 = ir1;
                const int i1  = i11;

                const char * __restrict__ src0_row   = static_cast<const char *>(up_data);
                const char * __restrict__ src3_row   = static_cast<const char *>(gate_data);
                const int  * __restrict__ router_row = (const int  *)((const char *)router_data + i11 * router_row_size);
                const char * __restrict__ src1_col   = reinterpret_cast<const char*>(wdata) + i11 * row_size;
                block_q8_0 * __restrict__ dst_col    = (block_q8_0 *)((char*)dst_ptr + i1 * nb1);

                float temp_buffer[QK8_0];
                for (int ir0 = iir0; ir0 < iir0 + blck_0 && ir0 < ir0_end; ir0++) {
                    const int router_idx = ir0 / T_n_ff;
                    const int inner_idx  = ir0 % T_n_ff;

                    const int expert_idx = router_row[router_idx];
                    const int row_idx    = expert_idx * T_n_ff + inner_idx;
                    const int dst_idx    = router_idx * T_n_ff + inner_idx;

                    float gate_val = 0;
                    vec_dot(T_n_embd, &gate_val, src3_row + row_idx * nb01, src1_col);

                    if (gate_val <= 0) { temp_buffer[ir0 - iir0] = 0; continue; }

                    float up_val   = 0;
                    vec_dot(T_n_embd, &up_val, src0_row + row_idx * nb01, src1_col);

                    if (up_val <= 0) { temp_buffer[ir0 - iir0] = 0; continue; }

                    temp_buffer[ir0 - iir0] = up_val * gate_val;
                }

                block_q8_0 * __restrict__ cur_dst_block = dst_col + iir0 / QK8_0;
                quantize_row_q8_0(temp_buffer, cur_dst_block, QK8_0);
            }
        }
    }
}

template<int T_n_ff, int T_n_embd>
inline void ggml_compute_forward_moe_sparse_one_chunk(
    const void *        __restrict__ weight_data,
    const block_q8_0 *  __restrict__ wdata,
    const int *         __restrict__ router_data,
    float *             __restrict__ dst_ptr,

    const int router_row_size,

    const int ir0_start,
    const int ir0_end,
    const int ir1_start,
    const int ir1_end) {

    // threads with no work simply yield (not sure if it helps)
    if (ir0_start >= ir0_end || ir1_start >= ir1_end) { return; }

    const     int row_size           = powerinfer_row_size(POWERINFER_TYPE_Q8_0, T_n_ff * SPARSE_MOE_NUM_HEAD);
    constexpr int expert_row_size    = powerinfer_row_size(POWERINFER_TYPE_Q4_0, T_n_ff);
    constexpr int expert_size        = expert_row_size * T_n_embd;

    // block-tiling attempt
    constexpr int blck_0 = 16;
    constexpr int blck_1 = 16;

    constexpr vec_dot_func_t vec_dot = ggml_vec_dot_q4_0_q8_0;

    // attempt to reduce false-sharing (does not seem to make a difference)
    // 16 * 2, accounting for mmla kernels
    for (int iir1 = ir1_start; iir1 < ir1_end; iir1 += blck_1) {
        for (int iir0 = ir0_start; iir0 < ir0_end; iir0 += blck_0) {
            for (int ir1 = iir1; ir1 < iir1 + blck_1 && ir1 < ir1_end; ir1++) {
                const int i11 = ir1;
                const int i1  = i11;
 
                const char * __restrict__ src0_row   = static_cast<const char *>(weight_data);
                const int  * __restrict__ router_row = reinterpret_cast<const int  *>(reinterpret_cast<const char *>(router_data) + i11 * router_row_size);
                const char * __restrict__ src1_col   = reinterpret_cast<const char*>(wdata) + i11 * row_size;
                float * __restrict__ dst_col         = dst_ptr + i1 * T_n_embd;


                for (int ir0 = iir0; ir0 < iir0 + blck_0 && ir0 < ir0_end; ir0++) {
                    float tmp = 0;
                    for (int router_id = 0; router_id < SPARSE_MOE_NUM_HEAD; ++router_id) {
                        const int expert_id     = router_row[router_id];
                        const int src1_offset   = powerinfer_row_size(POWERINFER_TYPE_Q8_0, router_id * T_n_ff);

                        const char *cur_src0_block = src0_row + expert_id * expert_size + ir0 * expert_row_size;

                        float vec_dot_val = 0;
                        vec_dot(T_n_ff, &vec_dot_val, cur_src0_block, src1_col + src1_offset);
                        tmp += vec_dot_val;
                    }
                    dst_col[ir0] = tmp;
                }
            }
        }
    }
}

template<int T_n_ff, int T_n_embd>
inline void ggml_compute_forward_mul_mat_up_gate_moe_sparse(
                    const int nth, std::atomic_int &counter,
                    const void *        __restrict__ up_data, 
                    const void *        __restrict__ gate_data,
                    const block_q8_0 *  __restrict__ input_data, 
                    const int *         __restrict__ router_ptr,
                    block_q8_0 *        __restrict__ dst_ptr,
                    const int batch_size, const int router_row_size) {

    // This is the size of the first dimension of the result, so we can iterate that way. (see the ASSERT above, these are the same numbers)
    constexpr int nr0 = T_n_ff * SPARSE_MOE_NUM_HEAD;

    // This is the size of the rest of the dimensions of the result
    const int nr1 = batch_size;

    // Now select a reasonable chunk size.
    constexpr int chunk_size = QK8_0;

    // distribute the work across the inner or outer loop based on which one is larger
    // The number of chunks in the 0/1 dim.
    // CEIL(nr0/chunk_size)
    int nchunk0 = (nr0 + chunk_size - 1) / chunk_size;
    int nchunk1 = (nr1 + chunk_size - 1) / chunk_size;

    if (nchunk0 * nchunk1 < nth * 4) {
        // distribute the thread work across the inner or outer loop based on which one is larger
        nchunk0 = nr0 > nr1 ? nth : 1; // parallelize by src0 rows
        nchunk1 = nr0 > nr1 ? 1 : nth; // parallelize by src1 rows
    }

    // The number of elements in each chunk
    const int dr0 = (nr0 + nchunk0 - 1) / nchunk0;
    const int dr1 = (nr1 + nchunk1 - 1) / nchunk1;

    int current_chunk = counter++;
    while (current_chunk < nchunk0 * nchunk1) {
        const int ith0 = current_chunk % nchunk0;
        const int ith1 = current_chunk / nchunk0;

        const int ir0_start = dr0 * ith0;
        const int ir0_end = std::min(ir0_start + dr0, nr0);

        const int ir1_start = dr1 * ith1;
        const int ir1_end = std::min(ir1_start + dr1, nr1);

        ggml_compute_forward_up_gate_moe_sparse_one_chunk<T_n_ff, T_n_embd>(up_data, gate_data, input_data, router_ptr, dst_ptr, 
            router_row_size,
            ir0_start, ir0_end, ir1_start, ir1_end);

        current_chunk = counter++;
    }
}

template<int T_n_ff, int T_n_embd>
inline void ggml_compute_forward_mul_mat_moe_sparse(
                    const int nth, std::atomic_int &counter,
                    const void *        __restrict__ down_data, 
                    const block_q8_0 *  __restrict__ input_data, 
                    const int *         __restrict__ router_ptr,
                    float *             __restrict__ dst_ptr,
                    const int batch_size, const int router_row_size) {

    // This is the size of the first dimension of the result, so we can iterate that way. (see the ASSERT above, these are the same numbers)
    constexpr int nr0 = T_n_embd;

    // This is the size of the rest of the dimensions of the result
    const int nr1 = batch_size;

    // Now select a reasonable chunk size.
    int chunk_size = 16;

    // We need to step up the size if it's small
    if (nr0 == 1 || nr1 == 1) { chunk_size = 64; }

    // distribute the work across the inner or outer loop based on which one is larger
    // The number of chunks in the 0/1 dim.
    // CEIL(nr0/chunk_size)
    int nchunk0 = (nr0 + chunk_size - 1) / chunk_size;
    int nchunk1 = (nr1 + chunk_size - 1) / chunk_size;

    // If the chunking is poor for the number of threads on this setup, scrap the whole plan.  Re-chunk it by thread.
    //   Also, chunking by thread was measured to have perform better on NUMA systems.  See https://github.com/ggerganov/llama.cpp/pull/6915
    //   In theory, chunking should be just as useful on NUMA and non NUMA systems, but testing disagreed with that.
    if (nchunk0 * nchunk1 < nth * 4) {
        // distribute the thread work across the inner or outer loop based on which one is larger
        nchunk0 = nr0 > nr1 ? nth : 1; // parallelize by src0 rows
        nchunk1 = nr0 > nr1 ? 1 : nth; // parallelize by src1 rows
    }

    // The number of elements in each chunk
    const int dr0 = (nr0 + nchunk0 - 1) / nchunk0;
    const int dr1 = (nr1 + nchunk1 - 1) / nchunk1;

    // The first chunk comes from our thread_id, the rest will get auto-assigned.
    int current_chunk = counter++;
    while (current_chunk < nchunk0 * nchunk1) {
        const int ith0 = current_chunk % nchunk0;
        const int ith1 = current_chunk / nchunk0;

        const int ir0_start = dr0 * ith0;
        const int ir0_end = std::min(ir0_start + dr0, nr0);

        const int ir1_start = dr1 * ith1;
        const int ir1_end = std::min(ir1_start + dr1, nr1);

        ggml_compute_forward_moe_sparse_one_chunk<T_n_ff, T_n_embd>(down_data, input_data, router_ptr, dst_ptr, 
            router_row_size,
            ir0_start, ir0_end, ir1_start, ir1_end);

        current_chunk = counter++;
    }
}