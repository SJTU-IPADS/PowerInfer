#include "az/cpu/quant_types.hpp"

#include "az/assert.hpp"

namespace az::cpu {

void quantize_row_q4_0(block_q4_0 *out, const float *in, size_t n) {
    static const int qk = block_q4_0::block_size;

    AZ_ASSERT(n % qk == 0);

    const size_t nb = n / qk;

    for (size_t i = 0; i < nb; i++) {
        float amax = 0.0f;  // absolute max
        float max = 0.0f;

        for (int j = 0; j < qk; j++) {
            const float v = in[i * qk + j];
            if (amax < fabsf(v)) {
                amax = fabsf(v);
                max = v;
            }
        }

        const float d = max / -8;
        const float id = d ? 1.0f / d : 0.0f;

        out[i].d = AZ_FP32_TO_FP16(d);

        for (int j = 0; j < qk / 2; ++j) {
            const float x0 = in[i * qk + 0 + j] * id;
            const float x1 = in[i * qk + qk / 2 + j] * id;

            const uint8_t xi0 = std::min((int8_t)15, (int8_t)(x0 + 8.5f));
            const uint8_t xi1 = std::min((int8_t)15, (int8_t)(x1 + 8.5f));

            out[i].qs[j] = xi0;
            out[i].qs[j] |= xi1 << 4;
        }
    }
}

void dequantize_row_q4_0(float *out, const block_q4_0 *in, size_t n) {
    static const size_t qk = block_q4_0::block_size;

    AZ_ASSERT(n % qk == 0);

    const size_t nb = n / qk;

    for (size_t i = 0; i < nb; i++) {
        const float d = AZ_FP16_TO_FP32(in[i].d);

        for (size_t j = 0; j < qk / 2; ++j) {
            const int x0 = (in[i].qs[j] & 0x0F) - 8;
            const int x1 = (in[i].qs[j] >> 4) - 8;

            out[i * qk + j + 0] = x0 * d;
            out[i * qk + j + qk / 2] = x1 * d;
        }
    }
}

void quantize_row_q8_0(block_q8_0 *out, const float *in, size_t n) {
    AZ_ASSERT(n % block_q8_0::block_size == 0);
    const int nb = n / block_q8_0::block_size;

#if defined(__ARM_NEON)
    for (int i = 0; i < nb; i++) {
        float32x4_t srcv[8];
        float32x4_t asrcv[8];
        float32x4_t amaxv[8];

        for (int j = 0; j < 8; j++)
            srcv[j] = vld1q_f32(in + i * 32 + 4 * j);
        for (int j = 0; j < 8; j++)
            asrcv[j] = vabsq_f32(srcv[j]);

        for (int j = 0; j < 4; j++)
            amaxv[2 * j] = vmaxq_f32(asrcv[2 * j], asrcv[2 * j + 1]);
        for (int j = 0; j < 2; j++)
            amaxv[4 * j] = vmaxq_f32(amaxv[4 * j], amaxv[4 * j + 2]);
        for (int j = 0; j < 1; j++)
            amaxv[8 * j] = vmaxq_f32(amaxv[8 * j], amaxv[8 * j + 4]);

        const float amax = vmaxvq_f32(amaxv[0]);

        const float d = amax / ((1 << 7) - 1);
        const float id = d ? 1.0f / d : 0.0f;

        out[i].d = AZ_FP32_TO_FP16(d);

        for (int j = 0; j < 8; j++) {
            const float32x4_t v = vmulq_n_f32(srcv[j], id);
            const int32x4_t vi = vcvtnq_s32_f32(v);

            out[i].qs[4 * j + 0] = vgetq_lane_s32(vi, 0);
            out[i].qs[4 * j + 1] = vgetq_lane_s32(vi, 1);
            out[i].qs[4 * j + 2] = vgetq_lane_s32(vi, 2);
            out[i].qs[4 * j + 3] = vgetq_lane_s32(vi, 3);
        }
    }
#elif defined(__AVX2__) || defined(__AVX__)
    for (int i = 0; i < nb; i++) {
        // Load elements into 4 AVX vectors
        __m256 v0 = _mm256_loadu_ps(in);
        __m256 v1 = _mm256_loadu_ps(in + 8);
        __m256 v2 = _mm256_loadu_ps(in + 16);
        __m256 v3 = _mm256_loadu_ps(in + 24);
        in += 32;

        // Compute max(abs(e)) for the block
        const __m256 signBit = _mm256_set1_ps(-0.0f);
        __m256 maxAbs = _mm256_andnot_ps(signBit, v0);
        maxAbs = _mm256_max_ps(maxAbs, _mm256_andnot_ps(signBit, v1));
        maxAbs = _mm256_max_ps(maxAbs, _mm256_andnot_ps(signBit, v2));
        maxAbs = _mm256_max_ps(maxAbs, _mm256_andnot_ps(signBit, v3));

        __m128 max4 = _mm_max_ps(_mm256_extractf128_ps(maxAbs, 1), _mm256_castps256_ps128(maxAbs));
        max4 = _mm_max_ps(max4, _mm_movehl_ps(max4, max4));
        max4 = _mm_max_ss(max4, _mm_movehdup_ps(max4));
        const float maxScalar = _mm_cvtss_f32(max4);

        // Quantize these floats
        const float d = maxScalar / 127.f;
        out[i].d = AZ_FP32_TO_FP16(d);
        const float id = (maxScalar != 0.0f) ? 127.f / maxScalar : 0.0f;
        const __m256 mul = _mm256_set1_ps(id);

        // Apply the multiplier
        v0 = _mm256_mul_ps(v0, mul);
        v1 = _mm256_mul_ps(v1, mul);
        v2 = _mm256_mul_ps(v2, mul);
        v3 = _mm256_mul_ps(v3, mul);

        // Round to nearest integer
        v0 = _mm256_round_ps(v0, _MM_ROUND_NEAREST);
        v1 = _mm256_round_ps(v1, _MM_ROUND_NEAREST);
        v2 = _mm256_round_ps(v2, _MM_ROUND_NEAREST);
        v3 = _mm256_round_ps(v3, _MM_ROUND_NEAREST);

        // Convert floats to integers
        __m256i i0 = _mm256_cvtps_epi32(v0);
        __m256i i1 = _mm256_cvtps_epi32(v1);
        __m256i i2 = _mm256_cvtps_epi32(v2);
        __m256i i3 = _mm256_cvtps_epi32(v3);

#if defined(__AVX2__)
        // Convert int32 to int16
        i0 = _mm256_packs_epi32(i0, i1);  // 0, 1, 2, 3,  8, 9, 10, 11,  4, 5, 6, 7, 12, 13, 14, 15
        i2 = _mm256_packs_epi32(i2, i3);  // 16, 17, 18, 19,  24, 25, 26, 27,  20, 21, 22, 23, 28, 29, 30, 31
                                          // Convert int16 to int8
        i0 = _mm256_packs_epi16(
            i0, i2
        );  // 0, 1, 2, 3,  8, 9, 10, 11,  16, 17, 18, 19,  24, 25, 26, 27,  4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31

        // We got our precious signed bytes, but the order is now wrong
        // These AVX2 pack instructions process 16-byte pieces independently
        // The following instruction is fixing the order
        const __m256i perm = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);
        i0 = _mm256_permutevar8x32_epi32(i0, perm);

        _mm256_storeu_si256((__m256i *)out[i].qs, i0);
#else   // __AVX2__
        // Since we don't have in AVX some necessary functions,
        // we split the registers in half and call AVX2 analogs from SSE
        __m128i ni0 = _mm256_castsi256_si128(i0);
        __m128i ni1 = _mm256_extractf128_si256(i0, 1);
        __m128i ni2 = _mm256_castsi256_si128(i1);
        __m128i ni3 = _mm256_extractf128_si256(i1, 1);
        __m128i ni4 = _mm256_castsi256_si128(i2);
        __m128i ni5 = _mm256_extractf128_si256(i2, 1);
        __m128i ni6 = _mm256_castsi256_si128(i3);
        __m128i ni7 = _mm256_extractf128_si256(i3, 1);

        // Convert int32 to int16
        ni0 = _mm_packs_epi32(ni0, ni1);
        ni2 = _mm_packs_epi32(ni2, ni3);
        ni4 = _mm_packs_epi32(ni4, ni5);
        ni6 = _mm_packs_epi32(ni6, ni7);
        // Convert int16 to int8
        ni0 = _mm_packs_epi16(ni0, ni2);
        ni4 = _mm_packs_epi16(ni4, ni6);

        _mm_storeu_si128((__m128i *)(out[i].qs + 0), ni0);
        _mm_storeu_si128((__m128i *)(out[i].qs + 16), ni4);
#endif  // __AVX2__
    }
#else
    abort();
#endif
}

}  // namespace az::cpu
