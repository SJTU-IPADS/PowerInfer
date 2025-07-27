#pragma once

#include <algorithm>
#include <cstddef>

#include "data.hpp"
#include "util/hyper.h"
#include "data.hpp"
#include "host/vec_dot.h"

inline void ggml_compute_forward_mul_mat_sparse_one_chunk(
    const void * __restrict__ weight_data, const block_q8_0 * __restrict__ wdata, const float * __restrict__ predictor_data,
    float * __restrict__ dst_ptr,

    const int64_t ne00, const int64_t ne01,

    const int64_t ir0_start,
    const int64_t ir0_end,
    const int64_t ir1_start,
    const int64_t ir1_end) {

    const int64_t nb01 = powerinfer_row_size(POWERINFER_TYPE_Q4_0, ne00);
    const int64_t nb1  = ne01 * sizeof(float);

    const int64_t predictor_row_size = sizeof(float) * ne01;

    // threads with no work simply yield (not sure if it helps)
    if (ir0_start >= ir0_end || ir1_start >= ir1_end) { return; }

    const size_t row_size = powerinfer_row_size(POWERINFER_TYPE_Q8_0, ne00);

    // block-tiling attempt
    const int64_t blck_0 = 16;
    const int64_t blck_1 = 16;

    // attempt to reduce false-sharing (does not seem to make a difference)
    // 16 * 2, accounting for mmla kernels
    for (int64_t iir1 = ir1_start; iir1 < ir1_end; iir1 += blck_1) {
        for (int64_t iir0 = ir0_start; iir0 < ir0_end; iir0 += blck_0) {
            for (int64_t ir1 = iir1; ir1 < iir1 + blck_1 && ir1 < ir1_end; ir1++) {
                const int64_t i11 = ir1;
                const int64_t i1  = i11;

                const char * src0_row   = (const char *)weight_data;
                const int  * predictor_row = (const int  *)((const char *)predictor_data + i11 * predictor_row_size);

                // desc: when src1 is not a contiguous memory block we have to calculate the offset using the strides
                //       if it is, then we have either copied the data to wdata and made it contiguous or we are using
                //       the original src1 data pointer, so we should index using the indices directly
                // TODO: this is a bit of a hack, we should probably have a better way to handle this
                const char * src1_col = (const char*)wdata + i11 * row_size;
                float * dst_col = (float*)((char*)dst_ptr + i1 * nb1);


                for (int64_t ir0 = iir0; ir0 < iir0 + blck_0 && ir0 < ir0_end; ir0++) {
                    const float score = predictor_row[ir0];
                    if (score <= SPARSE_PRED_THRESHOLD) { 
                        dst_col[ir0] = 0; 
                        continue;
                    }

                    ggml_vec_dot_q4_0_q8_0<32>(ne00, &dst_col[ir0], (const char *)src0_row + ir0 * nb01, src1_col);
                }
            }
        }
    }
}

inline void ggml_compute_forward_mul_mat_sparse(
                    const int ith, const int nth,
                    const void *weight_data, const block_q8_0 *input_data, const float *predictor_data,
                    float *dst_ptr,
                    const int64_t ne00, const int64_t ne01, const int64_t ne11) {

    // This is the size of the first dimension of the result, so we can iterate that way. (see the ASSERT above, these are the same numbers)
    const int64_t nr0 = ne01;

    // This is the size of the rest of the dimensions of the result
    const int64_t nr1 = ne11;

    // Now select a reasonable chunk size.
    int chunk_size = 16;

    // We need to step up the size if it's small
    if (nr0 == 1 || nr1 == 1) { chunk_size = 64; }

    // distribute the work across the inner or outer loop based on which one is larger
    // The number of chunks in the 0/1 dim.
    // CEIL(nr0/chunk_size)
    int64_t nchunk0 = (nr0 + chunk_size - 1) / chunk_size;
    int64_t nchunk1 = (nr1 + chunk_size - 1) / chunk_size;

    // If the chunking is poor for the number of threads on this setup, scrap the whole plan.  Re-chunk it by thread.
    //   Also, chunking by thread was measured to have perform better on NUMA systems.  See https://github.com/ggerganov/llama.cpp/pull/6915
    //   In theory, chunking should be just as useful on NUMA and non NUMA systems, but testing disagreed with that.
    if (nchunk0 * nchunk1 < nth * 4) {
        // distribute the thread work across the inner or outer loop based on which one is larger
        nchunk0 = nr0 > nr1 ? nth : 1; // parallelize by src0 rows
        nchunk1 = nr0 > nr1 ? 1 : nth; // parallelize by src1 rows
    }

    // The number of elements in each chunk
    const int64_t dr0 = (nr0 + nchunk0 - 1) / nchunk0;
    const int64_t dr1 = (nr1 + nchunk1 - 1) / nchunk1;

    //if (ith == 0)
    //    printf("MUL_MAT = [%d, %d, %d, %d] x [%d, %d, %d, %d] = %d x %d = %d.  Fp Ops/Ch %d\n", ne00, ne01, ne02, ne03, ne10, ne11, ne12, ne13, nchunk0, nchunk1, nchunk0 * nchunk1, ne00 * nr0 * nr1 / nchunk0 / nchunk1);

    // The first chunk comes from our thread_id, the rest will get auto-assigned.
    for (int current_chunk = ith; current_chunk < nchunk0 * nchunk1; current_chunk += nth) {
        const int64_t ith0 = current_chunk % nchunk0;
        const int64_t ith1 = current_chunk / nchunk0;

        const int64_t ir0_start = dr0 * ith0;
        const int64_t ir0_end = std::min(ir0_start + dr0, nr0);

        const int64_t ir1_start = dr1 * ith1;
        const int64_t ir1_end = std::min(ir1_start + dr1, nr1);

        ggml_compute_forward_mul_mat_sparse_one_chunk(weight_data, input_data, predictor_data, dst_ptr, 
            ne00, ne01,
            ir0_start, ir0_end, ir1_start, ir1_end);
    }
}