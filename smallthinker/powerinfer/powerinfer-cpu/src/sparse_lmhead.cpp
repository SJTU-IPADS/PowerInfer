#include <cstdint>
#include <atomic>

#include "convert.hpp"
#include "compare.hpp"
#include "vec_dot.hpp"
#include "powerinfer-cpu.hpp"

// 6-bit quantization
// weight is represented as x = a * q
// 16 blocks of 16 elements each
// Effectively 6.5625 bits per weight
// typedef struct {
//     uint8_t ql[QK_K/2];      // quants, lower 4 bits
//     uint8_t qh[QK_K/4];      // quants, upper 2 bits
//     int8_t  scales[QK_K/16]; // scales, quantized with 8 bits
//     ggml_fp16_t d;             // super-block scale
// } block_q6_K;
// static_assert(sizeof(block_q6_K) == sizeof(ggml_fp16_t) + QK_K / 16 + 3*QK_K/4, "wrong q6_K block size/padding");

// static __m128i get_scale_shuffle(int i) {
//     static const uint8_t k_shuffle[128] = {
//         0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
//         2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
//         4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
//         6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
//         8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,
//        10,10,10,10,10,10,10,10, 11,11,11,11,11,11,11,11,
//        12,12,12,12,12,12,12,12, 13,13,13,13,13,13,13,13,
//        14,14,14,14,14,14,14,14, 15,15,15,15,15,15,15,15
//    };
//     return _mm_loadu_si128((const __m128i*)k_shuffle + i);
// }

// static void ggml_vec_dot_q6_K_q8_K(int n, float * __restrict__ s, const void *__restrict__ vx, const void * __restrict__ vy) {

//     const block_q6_K * __restrict__ x = static_cast<const block_q6_K *>(vx);
//     const block_q8_K * __restrict__ y = static_cast<const block_q8_K *>(vy);

//     const int nb = n / QK_K;
//     const __m256i m4 = _mm256_set1_epi8(0xF);
//     const __m256i m2 = _mm256_set1_epi8(3);
//     const __m256i m32s = _mm256_set1_epi8(32);

//     __m256 acc = _mm256_setzero_ps();

//     for (int i = 0; i < nb; ++i) {

//         const float d = y[i].d * GGML_FP16_TO_FP32(x[i].d);

//         const uint8_t * __restrict__ q4 = x[i].ql;
//         const uint8_t * __restrict__ qh = x[i].qh;
//         const int8_t  * __restrict__ q8 = y[i].qs;

//         const __m128i scales = _mm_loadu_si128(reinterpret_cast<const __m128i *>(x[i].scales));

//         __m256i sumi = _mm256_setzero_si256();

//         int is = 0;

//         for (int j = 0; j < QK_K/128; ++j) {

//             const __m128i scale_0 = _mm_shuffle_epi8(scales, get_scale_shuffle(is + 0));
//             const __m128i scale_1 = _mm_shuffle_epi8(scales, get_scale_shuffle(is + 1));
//             const __m128i scale_2 = _mm_shuffle_epi8(scales, get_scale_shuffle(is + 2));
//             const __m128i scale_3 = _mm_shuffle_epi8(scales, get_scale_shuffle(is + 3));
//             is += 4;

//             const __m256i q4bits1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(q4)); q4 += 32;
//             const __m256i q4bits2 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(q4)); q4 += 32;
//             const __m256i q4bitsH = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(qh)); qh += 32;

//             const __m256i q4h_0 = _mm256_slli_epi16(_mm256_and_si256(q4bitsH, m2), 4);
//             const __m256i q4h_1 = _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(q4bitsH, 2), m2), 4);
//             const __m256i q4h_2 = _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(q4bitsH, 4), m2), 4);
//             const __m256i q4h_3 = _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(q4bitsH, 6), m2), 4);

//             const __m256i q4_0 = _mm256_or_si256(_mm256_and_si256(q4bits1, m4), q4h_0);
//             const __m256i q4_1 = _mm256_or_si256(_mm256_and_si256(q4bits2, m4), q4h_1);
//             const __m256i q4_2 = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi16(q4bits1, 4), m4), q4h_2);
//             const __m256i q4_3 = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi16(q4bits2, 4), m4), q4h_3);

//             const __m256i q8_0 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(q8)); q8 += 32;
//             const __m256i q8_1 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(q8)); q8 += 32;
//             const __m256i q8_2 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(q8)); q8 += 32;
//             const __m256i q8_3 = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(q8)); q8 += 32;

//             __m256i q8s_0 = _mm256_maddubs_epi16(m32s, q8_0);
//             __m256i q8s_1 = _mm256_maddubs_epi16(m32s, q8_1);
//             __m256i q8s_2 = _mm256_maddubs_epi16(m32s, q8_2);
//             __m256i q8s_3 = _mm256_maddubs_epi16(m32s, q8_3);

//             __m256i p16_0 = _mm256_maddubs_epi16(q4_0, q8_0);
//             __m256i p16_1 = _mm256_maddubs_epi16(q4_1, q8_1);
//             __m256i p16_2 = _mm256_maddubs_epi16(q4_2, q8_2);
//             __m256i p16_3 = _mm256_maddubs_epi16(q4_3, q8_3);

//             p16_0 = _mm256_sub_epi16(p16_0, q8s_0);
//             p16_1 = _mm256_sub_epi16(p16_1, q8s_1);
//             p16_2 = _mm256_sub_epi16(p16_2, q8s_2);
//             p16_3 = _mm256_sub_epi16(p16_3, q8s_3);

//             p16_0 = _mm256_madd_epi16(_mm256_cvtepi8_epi16(scale_0), p16_0);
//             p16_1 = _mm256_madd_epi16(_mm256_cvtepi8_epi16(scale_1), p16_1);
//             p16_2 = _mm256_madd_epi16(_mm256_cvtepi8_epi16(scale_2), p16_2);
//             p16_3 = _mm256_madd_epi16(_mm256_cvtepi8_epi16(scale_3), p16_3);

//             sumi = _mm256_add_epi32(sumi, _mm256_add_epi32(p16_0, p16_1));
//             sumi = _mm256_add_epi32(sumi, _mm256_add_epi32(p16_2, p16_3));

//         }

//         acc = _mm256_fmadd_ps(_mm256_broadcast_ss(&d), _mm256_cvtepi32_ps(sumi), acc);
//     }

//     *s = hsum_float_8(acc);
// }

template<powerinfer_type T_type>
    requires (T_type == POWERINFER_TYPE_Q6_K || T_type == POWERINFER_TYPE_Q4_0)
static void ggml_compute_forward_mul_mat_sparse_one_chunk(
    const void      * __restrict__ src0_data,
    const void      * __restrict__ src1_data,
    const uint32_t  * __restrict__ sparse_score_data,
          float     * __restrict__ dst_data,

    const int ne00, const int nb01,

    const int ir0_start,
    const int ir0_end) {

    if (ir0_start >= ir0_end) { return; }

    // block-tiling attempt
    constexpr int blck_0 = 16;

    // attempt to reduce false-sharing (does not seem to make a difference)
    // 16 * 2, accounting for mmla kernels
    float tmp[32];

    for (int iir0 = ir0_start; iir0 < ir0_end; iir0 += blck_0) {
        const char * __restrict__ src0_row         = static_cast<const char *>(src0_data);

        for (int ir0 = iir0; ir0 < iir0 + blck_0 && ir0 < ir0_end; ir0++) {
            const uint32_t score_num_idx = ir0 / 32;
            const uint32_t score_bit_idx = ir0 % 32;
            const uint32_t score_mask    = 1U << score_bit_idx;
            if ((sparse_score_data[score_num_idx] & score_mask) == 0) {
                tmp[ir0 - iir0] = 0;
                continue;
            }

            ggml_vec_dot_q4_0_q8_0(ne00, &tmp[ir0 - iir0], src0_row + ir0 * nb01, src1_data);
        }

        std::memcpy(&dst_data[iir0], tmp, (std::min(iir0 + blck_0, ir0_end) - iir0) * sizeof(float));
    }
}

template<powerinfer_type T_type>
    requires (T_type == POWERINFER_TYPE_Q6_K || T_type == POWERINFER_TYPE_Q4_0)
static PowerInferError powerinfer_host_lmhead_f32(const HostComputeParam param, std::atomic_int &counter,
        const uint32_t *__restrict__ profiler_output, 
        const void *    __restrict__ lmhead_data, 
        const void *    __restrict__ quantized_input_data, 
        float *         __restrict__ dst_data,
        const int num_embd, const int num_vocab) {

    const int       ith    = param.ith;
    const int       nth    = param.nth;

    const void * __restrict__ src0_data = lmhead_data;
    const int ne0  = num_embd;
    const int ne1  = num_vocab;
    const int nb11 = powerinfer_row_size(T_type, num_embd);

    // This is the size of the first dimension of the result, so we can iterate that way. (see the ASSERT above, these are the same numbers)
    const int nr0 = ne1;

    // Now select a reasonable chunk size.
    constexpr int chunk_size = 64;

    const int nchunk0 = (nr0 + chunk_size - 1) / chunk_size;

    // The number of elements in each chunk
    const int dr0 = (nr0 + nchunk0 - 1) / nchunk0;

    // The first chunk comes from our thread_id, the rest will get auto-assigned.
    int current_chunk = counter++;
    while (current_chunk < nchunk0) {
        const int ith0 = current_chunk % nchunk0;

        const int ir0_start = dr0 * ith0;
        const int ir0_end   = std::min(ir0_start + dr0, nr0);

        ggml_compute_forward_mul_mat_sparse_one_chunk<T_type>(src0_data, quantized_input_data, profiler_output, dst_data,
            ne0, nb11, ir0_start, ir0_end);

        current_chunk = counter++;
    }

    return { false, "Success" };
}

static void lmhead_profile(const HostComputeParam param, std::atomic_int &counter_up, std::atomic_int &counter_down, 
    const void * __restrict__ profiler_data, const void * __restrict__ input_data, uint32_t * __restrict__ output_ptr,
    const int num_embd, const int num_vocab, const int hidden_size) {

    const int ith = param.ith;
    const int nth = param.nth;

    block_q8_0 * __restrict__ middle_buffer     = reinterpret_cast<block_q8_0 *>(output_ptr + num_vocab);

    const size_t profiler_w1_row_size           = powerinfer_row_size(POWERINFER_TYPE_Q4_0, num_embd);
    const size_t profiler_w2_row_size           = powerinfer_row_size(POWERINFER_TYPE_Q4_0, hidden_size);
    const void * __restrict__ profiler_w1_data  = profiler_data;
    const void * __restrict__ profiler_w2_data  = static_cast<const char *>(profiler_data) + hidden_size * profiler_w1_row_size;

    const int num_hidden_block = hidden_size / QK8_0;
    POWERINFER_ASSERT(hidden_size % QK8_0 == 0);

    {
        int base_block_id = counter_up++;
        while (base_block_id < num_hidden_block) {
            float temp_buffer[QK8_0];

            const int base_row_id = base_block_id * QK8_0;
            for (int i = 0; i < QK8_0; ++i) {
                const void * __restrict__ cur_profiler_w1_row = static_cast<const char *>(profiler_w1_data) + (base_row_id + i) * profiler_w1_row_size;
                ggml_vec_dot_q4_0_q8_0(num_embd, &temp_buffer[i], cur_profiler_w1_row, input_data);
            }

            quantize_row_q8_0(temp_buffer, middle_buffer + base_block_id, QK8_0);

            base_block_id = counter_up++;
        }
    }

    param.arrive_and_wait();

    {
        constexpr int GROUP_NUM_ROW = 32;
        const int num_group         = num_vocab / GROUP_NUM_ROW;
        POWERINFER_ASSERT(num_vocab % GROUP_NUM_ROW == 0);

        float temp_buffer[GROUP_NUM_ROW];

        int base_group_id = counter_down++;
        while (base_group_id < num_group) {
            const int base_row_id = base_group_id * GROUP_NUM_ROW;
            for (int i = 0; i < GROUP_NUM_ROW; ++i) {
                const void * __restrict__ cur_profiler_w2_row = static_cast<const char *>(profiler_w2_data) + (base_row_id + i) * profiler_w2_row_size;
                ggml_vec_dot_q4_0_q8_0(hidden_size, &temp_buffer[i], cur_profiler_w2_row, middle_buffer);
            }

            constexpr int GROUP_NUM_BLOCK = GROUP_NUM_ROW / QK8_0;
            static_assert(GROUP_NUM_ROW % QK8_0 == 0);

            const int base_block_id       = base_group_id * GROUP_NUM_BLOCK;
            for (int i = 0; i < GROUP_NUM_BLOCK; ++i) {
                const uint32_t sparse_mask    = compare_fp32x32_array(temp_buffer + i * QK8_0, SPARSE_PRED_THRESHOLD);
                output_ptr[base_block_id + i] = sparse_mask;
            }

            base_group_id = counter_down++;
        }
    }
}


PowerInferError powerinfer_host_lmhead_q4_0_f32_impl(const HostComputeParam param,
    const void *profiler_data, const void *lmhead_data, const float *input_data, float *dst_data,
        const int num_embd, const int num_vocab, const int profiler_num_element) {

    const int       ith    = param.ith;
    const int       nth    = param.nth;
    void  *         wdata  = param.wdata;
    const size_t    wsize  = param.wsize;

    void  * dequantized_input              = wdata;
    const size_t dequantized_input_size    = powerinfer_row_size(POWERINFER_TYPE_Q8_0, num_embd);

    const int    hidden_size               = profiler_num_element / (num_embd + num_vocab);
    uint32_t * profiler_output             = reinterpret_cast<uint32_t *>(static_cast<char *>(wdata) + dequantized_input_size);
    const size_t profiler_buffer_size      = std::max(hidden_size, num_vocab) * sizeof(uint32_t);
    const size_t profiler_quant_size       = powerinfer_row_size(POWERINFER_TYPE_Q8_0, hidden_size);

    const size_t atomic_counter_align        = 64;
    const size_t atomic_counter_size         = atomic_counter_align + 3 * sizeof(std::atomic_int);
    const size_t atomic_counter_align_offset = (dequantized_input_size + profiler_quant_size + profiler_buffer_size + atomic_counter_align - 1) / atomic_counter_align * atomic_counter_align;
    std::atomic_int *atomic_counter_ptr      = reinterpret_cast<std::atomic_int *>(static_cast<char *>(wdata) + atomic_counter_align_offset);

    if (dequantized_input_size + profiler_quant_size + profiler_buffer_size + atomic_counter_size > wsize) { 
        return { true, "The size of compute buffer is too small" }; 
    }

    // quantize
    {
        constexpr int GROUP_NUM_BLOCK = 4;
        constexpr int GROUP_NUM_FP32  = GROUP_NUM_BLOCK * QK8_0;
        POWERINFER_ASSERT(num_embd % GROUP_NUM_FP32 == 0);

        for (int i = ith * GROUP_NUM_FP32; i < num_embd; i += nth * GROUP_NUM_FP32) {
            const float *      cur_input_group  = input_data + i;
            void        * cur_quant_input_group = static_cast<char *>(dequantized_input) + powerinfer_row_size(POWERINFER_TYPE_Q8_0, i);
            quantize_row_q8_0(cur_input_group, cur_quant_input_group, GROUP_NUM_FP32); 
        }        
    }

    if (ith == 0) { new (atomic_counter_ptr) std::atomic_int[3] {0, 0, 0}; }
    param.arrive_and_wait();

    lmhead_profile(param, atomic_counter_ptr[0], atomic_counter_ptr[1], 
        profiler_data, dequantized_input, profiler_output, num_embd, num_vocab, hidden_size);
    param.arrive_and_wait();

    return powerinfer_host_lmhead_f32<POWERINFER_TYPE_Q4_0>(param, atomic_counter_ptr[2],
        profiler_output, lmhead_data, dequantized_input, dst_data, num_embd, num_vocab);
}