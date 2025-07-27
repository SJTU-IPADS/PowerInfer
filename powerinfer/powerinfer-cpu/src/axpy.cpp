#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#if defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>
#endif
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include <cmath>

#include "powerinfer-cpu-data.hpp"
#include "axpy.hpp"


void az_axpy_fp16_act_fp32_batch_8_weight_q4_0(
    size_t n, size_t batch_size, const float a[8], const void * __restrict__ vx[8], ggml_fp16_t * __restrict__ y
) {
    POWERINFER_ASSERT(batch_size <= 8);

    constexpr size_t qk = 32;
    const size_t n_blocks = n / qk;
#if defined(__AVX2__) && defined(__FMA__) && defined(__F16C__)
    for (size_t i = 0; i < n_blocks; i++) {
        __m256 vy0 = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*)(y + i * qk + 0)));
        __m256 vy1 = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*)(y + i * qk + 8)));
        __m256 vy2 = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*)(y + i * qk + 16)));
        __m256 vy3 = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*)(y + i * qk + 24)));

        for (size_t j = 0; j < batch_size; j++) {
            const auto* row = static_cast<const block_q4_0*>(vx[j]) + i;
            const float d = POWERINFER_FP16_TO_FP32(row->d);
            const __m256 v_scale = _mm256_set1_ps(a[j] * d);

            const __m128i v_qs = _mm_loadu_si128((const __m128i*)row->qs);

            const __m128i m4b = _mm_set1_epi8(0x0F);
            const __m128i s8b = _mm_set1_epi8(8);
            const __m128i vx0_8 = _mm_sub_epi8(_mm_and_si128(v_qs, m4b), s8b);
            const __m128i vx1_8 = _mm_sub_epi8(_mm_and_si128(_mm_srli_epi16(v_qs, 4), m4b), s8b);
            
            __m256i v_dequant_s32_0 = _mm256_cvtepi8_epi32(vx0_8);
            vy0 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(v_dequant_s32_0), v_scale, vy0);
            
            __m256i v_dequant_s32_1 = _mm256_cvtepi8_epi32(_mm_unpackhi_epi64(vx0_8, _mm_setzero_si128()));
            vy1 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(v_dequant_s32_1), v_scale, vy1);

            __m256i v_dequant_s32_2 = _mm256_cvtepi8_epi32(vx1_8);
            vy2 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(v_dequant_s32_2), v_scale, vy2);

            __m256i v_dequant_s32_3 = _mm256_cvtepi8_epi32(_mm_unpackhi_epi64(vx1_8, _mm_setzero_si128()));
            vy3 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(v_dequant_s32_3), v_scale, vy3);
        }

        _mm_storeu_si128((__m128i*)(y + i * qk + 0),  _mm256_cvtps_ph(vy0, _MM_FROUND_TO_NEAREST_INT));
        _mm_storeu_si128((__m128i*)(y + i * qk + 8),  _mm256_cvtps_ph(vy1, _MM_FROUND_TO_NEAREST_INT));
        _mm_storeu_si128((__m128i*)(y + i * qk + 16), _mm256_cvtps_ph(vy2, _MM_FROUND_TO_NEAREST_INT));
        _mm_storeu_si128((__m128i*)(y + i * qk + 24), _mm256_cvtps_ph(vy3, _MM_FROUND_TO_NEAREST_INT));
    }
#elif defined(__ARM_NEON__) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    float16_t a_fp16[8];
    vst1_f16(a_fp16, vcvt_f16_f32(vld1q_f32(a)));
    vst1_f16(a_fp16 + 4, vcvt_f16_f32(vld1q_f32(a + 4)));
    
    const uint8x16_t m4b = vdupq_n_u8(0x0F);
    const int8x16_t s8b = vdupq_n_s8(8);

    for (size_t i = 0; i < n_blocks; i++) {
        float16_t *py = reinterpret_cast<float16_t *>(y + i * qk);
        float16x8_t vy0 = vld1q_f16(py + 0);
        float16x8_t vy1 = vld1q_f16(py + 8);
        float16x8_t vy2 = vld1q_f16(py + 16);
        float16x8_t vy3 = vld1q_f16(py + 24);

        for (size_t j = 0; j < batch_size; j++) {
            const auto* row = static_cast<const block_q4_0*>(vx[j]) + i;
            const float16_t d_val = vget_lane_f16(vld1_f16(reinterpret_cast<const float16_t*>(&row->d)), 0);
            const float16_t d_fp16 = a_fp16[j] * d_val;

            const uint8x16_t vx_qs = vld1q_u8(row->qs);
            const int8x16_t vx0 = vsubq_s8(vreinterpretq_s8_u8(vandq_u8(vx_qs, m4b)), s8b);
            const int8x16_t vx1 = vsubq_s8(vreinterpretq_s8_u8(vshrq_n_u8(vx_qs, 4)), s8b);

            vy0 = vfmaq_n_f16(vy0, vcvtq_f16_s16(vmovl_s8(vget_low_s8(vx0))), d_fp16);
            vy1 = vfmaq_n_f16(vy1, vcvtq_f16_s16(vmovl_high_s8(vx0)), d_fp16);
            vy2 = vfmaq_n_f16(vy2, vcvtq_f16_s16(vmovl_s8(vget_low_s8(vx1))), d_fp16);
            vy3 = vfmaq_n_f16(vy3, vcvtq_f16_s16(vmovl_high_s8(vx1)), d_fp16);
        }

        vst1q_f16(py + 0, vy0);
        vst1q_f16(py + 8, vy1);
        vst1q_f16(py + 16, vy2);
        vst1q_f16(py + 24, vy3);
    }
#else
    for (size_t i = 0; i < n_blocks; i++) {
        float tmp[qk];
        for (size_t p = 0; p < qk; p++) {
            tmp[p] = POWERINFER_FP16_TO_FP32(y[i * qk + p]);
        }
        
        for (size_t j = 0; j < batch_size; j++) {
            const auto* block = static_cast<const block_q4_0*>(vx[j]) + i;
            const float d = a[j] * POWERINFER_FP16_TO_FP32(block->d);
            for (size_t p = 0; p < qk / 2; p++) {
                const int x0 = (block->qs[p] & 0x0F) - 8;
                const int x1 = (block->qs[p] >> 4)  - 8;
                tmp[p]          += (float)x0 * d;
                tmp[p + qk / 2] += (float)x1 * d;
            }
        }

        for (size_t p = 0; p < qk; p++) {
            y[i * qk + p] = POWERINFER_FP32_TO_FP16(tmp[p]);
        }
    }
#endif
}


void az_axpy_fp32_act_fp32_batch_8_weight_q4_0(
    size_t n, size_t batch_size, const float a[8], const void * __restrict__ vx[8], float * __restrict__ y
) {
    POWERINFER_ASSERT(batch_size <= 8);

    constexpr size_t qk = 32;
    const size_t n_blocks = n / qk;

#if defined(__AVX2__) && defined(__FMA__)
    for (size_t i = 0; i < n_blocks; i++) {
        __m256 vy0 = _mm256_setzero_ps(); 
        __m256 vy1 = _mm256_setzero_ps();
        __m256 vy2 = _mm256_setzero_ps(); 
        __m256 vy3 = _mm256_setzero_ps();

        for (size_t j = 0; j < batch_size; j++) {
            const auto* block = static_cast<const block_q4_0*>(vx[j]) + i;
            const __m256 vd = _mm256_set1_ps(a[j] * POWERINFER_FP16_TO_FP32(block->d));
            const __m128i qs = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block->qs));

            const __m128i m4b = _mm_set1_epi8(0x0f);
            const __m128i s8b = _mm_set1_epi8(8);
            const __m128i q_lo = _mm_sub_epi8(_mm_and_si128(qs, m4b), s8b);
            const __m128i q_hi = _mm_sub_epi8(_mm_and_si128(_mm_srli_epi16(qs, 4), m4b), s8b);

            __m128i q_lo_hi_half = _mm_unpackhi_epi64(q_lo, _mm_setzero_si128());
            __m128i q_hi_hi_half = _mm_unpackhi_epi64(q_hi, _mm_setzero_si128());

            vy0 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(q_lo)), vd, vy0);
            vy1 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(q_lo_hi_half)), vd, vy1);
            vy2 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(q_hi)), vd, vy2);
            vy3 = _mm256_fmadd_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(q_hi_hi_half)), vd, vy3);
        }

        float* py = y + i * qk;
        _mm256_storeu_ps(py,      _mm256_add_ps(_mm256_loadu_ps(py),      vy0));
        _mm256_storeu_ps(py + 8,  _mm256_add_ps(_mm256_loadu_ps(py + 8),  vy1));
        _mm256_storeu_ps(py + 16, _mm256_add_ps(_mm256_loadu_ps(py + 16), vy2));
        _mm256_storeu_ps(py + 24, _mm256_add_ps(_mm256_loadu_ps(py + 24), vy3));
    }
#elif defined(__ARM_NEON)
    const uint8x16_t m4b = vdupq_n_u8(0x0f);
    const int8x16_t s8b = vdupq_n_s8(8);

    auto accumulate = [](float* y, int16x8_t x, float scale) {
        float32x4_t vy0 = vld1q_f32(y);
        float32x4_t vx0 = vcvtq_f32_s32(vmovl_s16(vget_low_s16(x)));
        vst1q_f32(y, vfmaq_n_f32(vy0, vx0, scale));

        float32x4_t vy1 = vld1q_f32(y + 4);
        float32x4_t vx1 = vcvtq_f32_s32(vmovl_high_s16(x));
        vst1q_f32(y + 4, vfmaq_n_f32(vy1, vx1, scale));
    };

    for (size_t i = 0; i < n_blocks; i++) {
        float d_arr[8] = {};
        for (size_t j = 0; j < batch_size; j++) {
            const auto* block = static_cast<const block_q4_0*>(vx[j]) + i;
            d_arr[j] = a[j] * POWERINFER_FP16_TO_FP32(block->d);
        }

        float32x4_t vd0 = vld1q_f32(d_arr);
        float32x4_t vd1 = vld1q_f32(d_arr + 4);
        float amax = fmaxf(vmaxvq_f32(vabsq_f32(vd0)), vmaxvq_f32(vabsq_f32(vd1)));
        float scale = (amax != 0.0f) ? (amax / 127.0f) : 0.0f;
        float inverted_scale = (amax != 0.0f) ? (127.0f / amax) : 0.0f;

        int16x8_t vy0 = vdupq_n_s16(0), vy1 = vdupq_n_s16(0);
        int16x8_t vy2 = vdupq_n_s16(0), vy3 = vdupq_n_s16(0);

        for (size_t j = 0; j < batch_size; j++) {
            int8x8_t quant_vd = vdup_n_s8(static_cast<int8_t>(roundf(d_arr[j] * inverted_scale)));
            const auto* block = static_cast<const block_q4_0*>(vx[j]) + i;
            uint8x16_t vx_qs = vld1q_u8(block->qs);

            int8x16_t vx0 = vsubq_s8(vreinterpretq_s8_u8(vandq_u8(vx_qs, m4b)), s8b);
            vy0 = vmlal_s8(vy0, vget_low_s8(vx0), quant_vd);
            vy1 = vmlal_s8(vy1, vget_high_s8(vx0), quant_vd);

            int8x16_t vx1 = vsubq_s8(vreinterpretq_s8_u8(vshrq_n_u8(vx_qs, 4)), s8b);
            vy2 = vmlal_s8(vy2, vget_low_s8(vx1), quant_vd);
            vy3 = vmlal_s8(vy3, vget_high_s8(vx1), quant_vd);
        }

        float* py = y + i * qk;
        accumulate(py, vy0, scale);
        accumulate(py + 8, vy1, scale);
        accumulate(py + 16, vy2, scale);
        accumulate(py + 24, vy3, scale);
    }
#else
    for (size_t i = 0; i < n_blocks; i++) {
        float tmp[qk] = {};
        for (size_t j = 0; j < batch_size; j++) {
            const auto* block = static_cast<const block_q4_0*>(vx[j]) + i;
            const float d = a[j] * POWERINFER_FP16_TO_FP32(block->d);
            for (size_t p = 0; p < qk / 2; p++) {
                tmp[p]           += (float)((block->qs[p] & 0x0F) - 8) * d;
                tmp[p + qk / 2]  += (float)((block->qs[p] >> 4) - 8) * d;
            }
        }
        float* py = y + i * qk;
        for (size_t p = 0; p < qk; p++) {
            py[p] += tmp[p];
        }
    }
#endif
}

template <typename T>
static inline void axpy_reduce_f32_impl(size_t vec_dim, size_t n_inputs, const T *inputs[], float *output,bool do_accumulate,float scale);

template <>
void axpy_reduce_f32_impl<ggml_fp16_t>(size_t vec_dim, size_t n_inputs, const ggml_fp16_t *inputs[], float *output,bool do_accumulate, float scale) {
#if defined(__ARM_NEON)
    const size_t stride = 4; // Process 4 elements of ggml_fp16_t at a time

    POWERINFER_ASSERT(vec_dim % stride == 0);

    // Hoist the scale factor duplication out of the loop
    const float32x4_t scale_neon_f32 = vdupq_n_f32(scale); // 'scale' comes from the function signature, like in #else

    for (size_t i = 0; i < vec_dim; i += stride) {
        float32x4_t acc_neon_f32;
        size_t k_loop_start_index;

        if (do_accumulate) {
            acc_neon_f32 = vld1q_f32(output + i);
            k_loop_start_index = 0; // Loop from inputs[0]
        } else { 
            float16x4_t y0_f16 = vld1_f16((const float16_t *)(inputs[0] + i));
            float32x4_t y0_f32 = vcvt_f32_f16(y0_f16);
            acc_neon_f32 = vmulq_f32(y0_f32, scale_neon_f32);
            k_loop_start_index = 1;
        }

        for (size_t k = k_loop_start_index; k < n_inputs; k++) {
            float16x4_t yk_f16 = vld1_f16((const float16_t *)(inputs[k] + i));
            float32x4_t yk_f32 = vcvt_f32_f16(yk_f16); 
           
            float32x4_t term_neon_f32 = vmulq_f32(yk_f32, scale_neon_f32);
            acc_neon_f32 = vaddq_f32(acc_neon_f32, term_neon_f32);
        }

        vst1q_f32(output + i, acc_neon_f32); 
    }
#else
    for (size_t i = 0; i < vec_dim; i++) {
        if(do_accumulate)
        {
            for (size_t k = 0; k < n_inputs; k++) {
                output[i] += POWERINFER_FP16_TO_FP32(inputs[k][i])*scale;
            }
        }
        else
        {
            output[i] = POWERINFER_FP16_TO_FP32(inputs[0][i])*scale;
            for (size_t k = 1; k < n_inputs; k++) {
                output[i] += POWERINFER_FP16_TO_FP32(inputs[k][i])*scale;
            }
        }
    }
#endif
}

template <>
void axpy_reduce_f32_impl<float>(size_t vec_dim, size_t n_inputs, const float *inputs[], float *output,bool do_accumulate,float scale) {
#if defined(__ARM_NEON)
    const size_t stride = 4;

    POWERINFER_ASSERT(vec_dim % stride == 0);

    const float32x4_t scale_neon = vdupq_n_f32(scale);

    for (size_t i = 0; i < vec_dim; i += stride) {
        float32x4_t acc_neon; 
        size_t k_loop_start_index;

        if (do_accumulate) {
            acc_neon = vld1q_f32(output + i);
            k_loop_start_index = 0;
        } else { 
            float32x4_t y0_neon = vld1q_f32(inputs[0] + i);
            acc_neon = vmulq_f32(y0_neon, scale_neon);
            k_loop_start_index = 1; 
        }

        for (size_t k = k_loop_start_index; k < n_inputs; k++) {
            float32x4_t yk_neon = vld1q_f32(inputs[k] + i); 
            acc_neon = vfmaq_f32(acc_neon, yk_neon, scale_neon);
        }

        vst1q_f32(output + i, acc_neon); 
    }
#else
    for (size_t i = 0; i < vec_dim; i++) {
        if(do_accumulate)
        {
            for (size_t k = 0; k < n_inputs; k++) {
                output[i] += (inputs[k][i]*scale);
            }
        }
        else{
            output[i] = inputs[0][i]*scale;
            for (size_t k = 1; k < n_inputs; k++) {
                output[i] += (inputs[k][i]*scale);
            }
        }
    }
#endif
}

void axpy_reduce_f32(size_t vec_dim, size_t n_inputs, const axpy_out_type *inputs[], float *output,bool do_accumulate,float scale) {
    axpy_reduce_f32_impl<axpy_out_type>(vec_dim, n_inputs, inputs, output,do_accumulate,scale);
}

void AxpyBatch::enqueue(float a, const void *vx) {
        if (batch_size >= axpy_batch_size) {
            flush();
        }

        size_t i = batch_size++;
        this->a[i] = a;
        this->vx[i] = vx;
}

void AxpyBatch::flush() {
    if (batch_size == 0) {
        return;
    }

#if defined(AXPY_USE_FP16)
    az_axpy_fp16_act_fp32_batch_8_weight_q4_0(vec_dim, batch_size, a, vx, vy);
#else
    az_axpy_fp32_act_fp32_batch_8_weight_q4_0(vec_dim, batch_size, a, vx, vy);
#endif

    batch_size = 0;
}
