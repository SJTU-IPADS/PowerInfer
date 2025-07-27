#include "az/cpu/vec_dot.hpp"

#include "az/assert.hpp"
#include "az/cpu/vdot.hpp"

#include <sys/prctl.h>

#if defined(__ARM_FEATURE_SVE)
static int sve_cnt = PR_SVE_VL_LEN_MASK & prctl(PR_SVE_GET_VL);
#endif

#if defined(__AVX2__)

namespace {

// multiply int8_t, add results pairwise twice
inline __m128i mul_sum_i8_pairs(const __m128i x, const __m128i y) {
    // Get absolute values of x vectors
    const __m128i ax = _mm_sign_epi8(x, x);
    // Sign the values of the y vectors
    const __m128i sy = _mm_sign_epi8(y, x);
    // Perform multiplication and create 16-bit values
    const __m128i dot = _mm_maddubs_epi16(ax, sy);
    const __m128i ones = _mm_set1_epi16(1);
    return _mm_madd_epi16(ones, dot);
}

// horizontally add 8 floats
inline float hsum_float_8(const __m256 x) {
    __m128 res = _mm256_extractf128_ps(x, 1);
    res = _mm_add_ps(res, _mm256_castps256_ps128(x));
    res = _mm_add_ps(res, _mm_movehl_ps(res, res));
    res = _mm_add_ss(res, _mm_movehdup_ps(res));
    return _mm_cvtss_f32(res);
}

// Unpack 32 4-bit fields into 32 bytes
// The output vector contains 32 bytes, each one in [ 0 .. 15 ] interval
inline __m256i bytes_from_nibbles_32(const uint8_t *rsi) {
    const __m128i tmp = _mm_loadu_si128((const __m128i *)rsi);
    const __m256i bytes = _mm256_set_m128i(_mm_srli_epi16(tmp, 4), tmp);
    const __m256i lowMask = _mm256_set1_epi8(0xF);
    return _mm256_and_si256(lowMask, bytes);
}

// add int16_t pairwise and return as float vector
inline __m256 sum_i16_pairs_float(const __m256i x) {
    const __m256i ones = _mm256_set1_epi16(1);
    const __m256i summed_pairs = _mm256_madd_epi16(ones, x);
    return _mm256_cvtepi32_ps(summed_pairs);
}

inline __m256 mul_sum_us8_pairs_float(const __m256i ax, const __m256i sy) {
#if defined(__AVXVNNI__) || (defined(__AVX512VNNI__) && defined(__AVX512VL__))
    const __m256i zero = _mm256_setzero_si256();
    const __m256i summed_pairs = _mm256_dpbusd_epi32(zero, ax, sy);
    return _mm256_cvtepi32_ps(summed_pairs);
#else
    // Perform multiplication and create 16-bit values
    const __m256i dot = _mm256_maddubs_epi16(ax, sy);
    return sum_i16_pairs_float(dot);
#endif
}

inline __m256 mul_sum_i8_pairs_float(const __m256i x, const __m256i y) {
#if __AVXVNNIINT8__
    const __m256i zero = _mm256_setzero_si256();
    const __m256i summed_pairs = _mm256_dpbssd_epi32(zero, x, y);
    return _mm256_cvtepi32_ps(summed_pairs);
#else
    // Get absolute values of x vectors
    const __m256i ax = _mm256_sign_epi8(x, x);
    // Sign the values of the y vectors
    const __m256i sy = _mm256_sign_epi8(y, x);
    return mul_sum_us8_pairs_float(ax, sy);
#endif
}

}

#endif  // __AVX2__

namespace az::cpu {

float vec_dot_q4_0_q8_0(size_t n, const void *__restrict__ vx, const void * __restrict__ vy) {
    const block_q4_0 *__restrict__ x = static_cast<const block_q4_0 *>(vx);
    const block_q8_0 *__restrict__ y = static_cast<const block_q8_0 *>(vy);
    
    const size_t qk = block_q8_0::block_size;
    const size_t nb = n / qk;
    AZ_ASSERT(qk * nb == n);

    size_t ib = 0;
    float sumf = 0;

#if 0
    svfloat32_t sumv0 = svdup_n_f32(0.0f);
    svfloat32_t sumv1 = svdup_n_f32(0.0f);

    const int vector_length = sve_cnt * 8;

    // VLA Implementation using switch case
    switch (vector_length) {
    case 128: {
        // predicate for activating higher lanes for 4 float32 elements
        const svbool_t ph4 = svptrue_pat_b32(SV_VL4);

        for (; ib + 1 < nb; ib += 2) {
            const block_q4_0 *__restrict__ x0 = &x[ib + 0];
            const block_q4_0 *__restrict__ x1 = &x[ib + 1];
            const block_q8_0 *__restrict__ y0 = &y[ib + 0];
            const block_q8_0 *__restrict__ y1 = &y[ib + 1];

            // load x
            const svuint8_t qx0r = svld1rq_u8(svptrue_b8(), x0->qs);
            const svuint8_t qx1r = svld1rq_u8(svptrue_b8(), x1->qs);

            // 4-bit -> 8-bit
            const svint8_t qx0l = svreinterpret_s8_u8(svand_n_u8_m(svptrue_b8(), qx0r, 0x0F));
            const svint8_t qx0h = svreinterpret_s8_u8(svlsr_n_u8_m(svptrue_b8(), qx0r, 0x04));
            const svint8_t qx1l = svreinterpret_s8_u8(svand_n_u8_m(svptrue_b8(), qx1r, 0x0F));
            const svint8_t qx1h = svreinterpret_s8_u8(svlsr_n_u8_m(svptrue_b8(), qx1r, 0x04));

            // sub 8
            const svint8_t qx0ls = svsub_n_s8_x(svptrue_b8(), qx0h, 8);
            const svint8_t qx0hs = svsub_n_s8_x(svptrue_b8(), qx0l, 8);
            const svint8_t qx1ls = svsub_n_s8_x(svptrue_b8(), qx1h, 8);
            const svint8_t qx1hs = svsub_n_s8_x(svptrue_b8(), qx1l, 8);

            // load y
            const svint8_t qy0h = svld1_s8(svptrue_b8(), y0->qs);
            const svint8_t qy0l = svld1_s8(svptrue_b8(), y0->qs + 16);
            const svint8_t qy1h = svld1_s8(svptrue_b8(), y1->qs);
            const svint8_t qy1l = svld1_s8(svptrue_b8(), y1->qs + 16);

            // dot product
            sumv0 = svmla_n_f32_x(
                ph4,
                sumv0,
                svcvt_f32_s32_x(
                    ph4, svadd_x(ph4, svdot_s32(svdup_n_s32(0), qx0ls, qy0l), svdot_s32(svdup_n_s32(0), qx0hs, qy0h))
                ),
                AZ_FP16_TO_FP32(x0->d) * AZ_FP16_TO_FP32(y0->d)
            );
            sumv1 = svmla_n_f32_x(
                ph4,
                sumv1,
                svcvt_f32_s32_x(
                    ph4, svadd_x(ph4, svdot_s32(svdup_n_s32(0), qx1ls, qy1l), svdot_s32(svdup_n_s32(0), qx1hs, qy1h))
                ),
                AZ_FP16_TO_FP32(x1->d) * AZ_FP16_TO_FP32(y1->d)
            );
        }

        sumf = svaddv_f32(svptrue_b32(), svadd_f32_x(svptrue_b32(), sumv0, sumv1));
    } break;
    case 256: {
        // predicate for activating higher lanes for 16 int8 elements
        const svbool_t ph16 = svptrue_pat_b8(SV_VL16);
        // predicate for activating lower lanes for  16 int8 elements
        const svbool_t pl16 = svnot_b_z(svptrue_b8(), ph16);

        for (; ib + 1 < nb; ib += 2) {
            const block_q4_0 *__restrict__ x0 = &x[ib + 0];
            const block_q4_0 *__restrict__ x1 = &x[ib + 1];
            const block_q8_0 *__restrict__ y0 = &y[ib + 0];
            const block_q8_0 *__restrict__ y1 = &y[ib + 1];

            // load x
            const svuint8_t qx0r = svld1rq_u8(svptrue_b8(), x0->qs);
            const svuint8_t qx1r = svld1rq_u8(svptrue_b8(), x1->qs);

            // 4-bit -> 8-bit
            const svint8_t qx0 = svreinterpret_s8_u8(svlsr_n_u8_m(pl16, svand_n_u8_m(ph16, qx0r, 0x0F), 0x04));
            const svint8_t qx1 = svreinterpret_s8_u8(svlsr_n_u8_m(pl16, svand_n_u8_m(ph16, qx1r, 0x0F), 0x04));

            // sub 8
            const svint8_t qx0s = svsub_n_s8_x(svptrue_b8(), qx0, 8);
            const svint8_t qx1s = svsub_n_s8_x(svptrue_b8(), qx1, 8);

            // load y
            const svint8_t qy0 = svld1_s8(svptrue_b8(), y0->qs);
            const svint8_t qy1 = svld1_s8(svptrue_b8(), y1->qs);

            // dot product
            sumv0 = svmla_n_f32_x(
                svptrue_b32(),
                sumv0,
                svcvt_f32_s32_x(svptrue_b32(), svdot_s32(svdup_n_s32(0), qx0s, qy0)),
                AZ_FP16_TO_FP32(x0->d) * AZ_FP16_TO_FP32(y0->d)
            );
            sumv1 = svmla_n_f32_x(
                svptrue_b32(),
                sumv1,
                svcvt_f32_s32_x(svptrue_b32(), svdot_s32(svdup_n_s32(0), qx1s, qy1)),
                AZ_FP16_TO_FP32(x1->d) * AZ_FP16_TO_FP32(y1->d)
            );
        }

        sumf = svaddv_f32(svptrue_b32(), svadd_f32_x(svptrue_b32(), sumv0, sumv1));
    } break;
    case 512: {
        // predicate for activating higher lanes for 32 int8 elements
        const svbool_t ph32 = svptrue_pat_b8(SV_VL32);

        // predicate for activating higher lanes for 16 int8 elements
        const svbool_t ph16 = svptrue_pat_b8(SV_VL16);
        // predicate for activating lower lanes for 16 int8 elements from first 32 int8 activated lanes
        const svbool_t pl16 = svnot_b_z(ph32, ph16);

        for (; ib + 1 < nb; ib += 2) {
            const block_q4_0 *__restrict__ x0 = &x[ib + 0];
            const block_q4_0 *__restrict__ x1 = &x[ib + 1];
            const block_q8_0 *__restrict__ y0 = &y[ib + 0];
            const block_q8_0 *__restrict__ y1 = &y[ib + 1];

            // load x
            const svuint8_t qx0r = svld1rq_u8(ph32, x0->qs);
            const svuint8_t qx1r = svld1rq_u8(ph32, x1->qs);

            // 4-bit -> 8-bit
            const svint8_t qx0 = svreinterpret_s8_u8(svlsr_n_u8_m(pl16, svand_n_u8_m(ph16, qx0r, 0x0F), 0x04));
            const svint8_t qx1 = svreinterpret_s8_u8(svlsr_n_u8_m(pl16, svand_n_u8_m(ph16, qx1r, 0x0F), 0x04));

            // sub 8
            const svint8_t qx0s = svsub_n_s8_x(ph32, qx0, 8);
            const svint8_t qx1s = svsub_n_s8_x(ph32, qx1, 8);

            // load y
            const svint8_t qy0 = svld1_s8(ph32, y0->qs);
            const svint8_t qy1 = svld1_s8(ph32, y1->qs);

            // dot product
            sumv0 = svmla_n_f32_x(
                ph32,
                sumv0,
                svcvt_f32_s32_x(ph32, svdot_s32(svdup_n_s32(0), qx0s, qy0)),
                AZ_FP16_TO_FP32(x0->d) * AZ_FP16_TO_FP32(y0->d)
            );
            sumv1 = svmla_n_f32_x(
                ph32,
                sumv1,
                svcvt_f32_s32_x(ph32, svdot_s32(svdup_n_s32(0), qx1s, qy1)),
                AZ_FP16_TO_FP32(x1->d) * AZ_FP16_TO_FP32(y1->d)
            );
        }

        sumf = svaddv_f32(ph32, svadd_f32_x(ph32, sumv0, sumv1));
    } break;
    default:
        AZ_ASSERT(false && "Unsupported vector length");
        break;
    }

#elif defined(__ARM_NEON)
    float32x4_t sumv0 = vdupq_n_f32(0.0f);
    float32x4_t sumv1 = vdupq_n_f32(0.0f);

    for (; ib + 1 < nb; ib += 2) {
        const block_q4_0 *__restrict__ x0 = &x[ib + 0];
        const block_q4_0 *__restrict__ x1 = &x[ib + 1];
        const block_q8_0 *__restrict__ y0 = &y[ib + 0];
        const block_q8_0 *__restrict__ y1 = &y[ib + 1];

        const uint8x16_t m4b = vdupq_n_u8(0x0F);
        const int8x16_t s8b = vdupq_n_s8(0x8);

        const uint8x16_t v0_0 = vld1q_u8(x0->qs);
        const uint8x16_t v0_1 = vld1q_u8(x1->qs);

        // 4-bit -> 8-bit
        const int8x16_t v0_0l = vreinterpretq_s8_u8(vandq_u8(v0_0, m4b));
        const int8x16_t v0_0h = vreinterpretq_s8_u8(vshrq_n_u8(v0_0, 4));
        const int8x16_t v0_1l = vreinterpretq_s8_u8(vandq_u8(v0_1, m4b));
        const int8x16_t v0_1h = vreinterpretq_s8_u8(vshrq_n_u8(v0_1, 4));

        // sub 8
        const int8x16_t v0_0ls = vsubq_s8(v0_0l, s8b);
        const int8x16_t v0_0hs = vsubq_s8(v0_0h, s8b);
        const int8x16_t v0_1ls = vsubq_s8(v0_1l, s8b);
        const int8x16_t v0_1hs = vsubq_s8(v0_1h, s8b);

        // const int8x16_t v0_0ls = vshlq_n_s8  (v0_0, 4);
        // const int8x16_t v0_0hs = (vandq_u8(v0_0, m4b));
        // const int8x16_t v0_1ls = vshlq_n_s8  (v0_1, 4);
        // const int8x16_t v0_1hs = (vandq_u8(v0_1, m4b));

        // load y
        const int8x16x2_t v1_0 = vld1q_s8_x2(y0->qs);
        //const int8x16_t v1_0h = vld1q_s8(y0->qs + 16);
        const int8x16x2_t v1_1 = vld1q_s8_x2(y1->qs);
        //const int8x16_t v1_1h = vld1q_s8(y1->qs + 16);

        // dot product into int32x4_t
        const int32x4_t p_0 = vdotq_s32_impl(vdotq_s32_impl(vdupq_n_s32(0), v0_0ls, v1_0.val[0]), v0_0hs, v1_0.val[1]);
        const int32x4_t p_1 = vdotq_s32_impl(vdotq_s32_impl(vdupq_n_s32(0), v0_1ls, v1_1.val[0]), v0_1hs, v1_1.val[1]);

        sumv0 = vmlaq_n_f32(sumv0, vcvtq_f32_s32(p_0), AZ_FP16_TO_FP32(x0->d) * AZ_FP16_TO_FP32(y0->d));
        sumv1 = vmlaq_n_f32(sumv1, vcvtq_f32_s32(p_1), AZ_FP16_TO_FP32(x1->d) * AZ_FP16_TO_FP32(y1->d));
    }

    sumf = vaddvq_f32(sumv0) + vaddvq_f32(sumv1);
    //sumf/=16;
#elif defined(__AVX2__)
    // Initialize accumulator with zeros
    __m256 acc = _mm256_setzero_ps();

    // Main loop
    for (; ib < nb; ++ib) {
        /* Compute combined scale for the block */
        const __m256 d = _mm256_set1_ps(AZ_FP16_TO_FP32(x[ib].d) * AZ_FP16_TO_FP32(y[ib].d));

        __m256i qx = bytes_from_nibbles_32(x[ib].qs);

        // Now we have a vector with bytes in [ 0 .. 15 ] interval. Offset them into [ -8 .. +7 ] interval.
        const __m256i off = _mm256_set1_epi8(8);
        qx = _mm256_sub_epi8(qx, off);

        __m256i qy = _mm256_loadu_si256((const __m256i *)y[ib].qs);

        const __m256 q = mul_sum_i8_pairs_float(qx, qy);

        /* Multiply q with scale and accumulate */
        acc = _mm256_fmadd_ps(d, q, acc);
    }

    sumf = hsum_float_8(acc);
#elif defined(__AVX__)
    __m256 accum = _mm256_setzero_ps();
    for (; ib + 1 < nb; ib += 2) {
        const __m128i q4bits_1 = _mm_loadu_si128((const __m128i *)x[ib + 0].qs);
        const __m128i q4bits_2 = _mm_loadu_si128((const __m128i *)x[ib + 1].qs);
        const __m128i q8b_1_0 = _mm_loadu_si128((const __m128i *)y[ib + 0].qs);
        const __m128i q8b_1_1 = _mm_loadu_si128((const __m128i *)y[ib + 0].qs + 1);
        const __m128i q8b_2_0 = _mm_loadu_si128((const __m128i *)y[ib + 1].qs);
        const __m128i q8b_2_1 = _mm_loadu_si128((const __m128i *)y[ib + 1].qs + 1);

        const __m128i q4b_1_0 = _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), q4bits_1), _mm_set1_epi8(8));
        const __m128i q4b_1_1 =
            _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), _mm_srli_epi16(q4bits_1, 4)), _mm_set1_epi8(8));
        const __m128i q4b_2_0 = _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), q4bits_2), _mm_set1_epi8(8));
        const __m128i q4b_2_1 =
            _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), _mm_srli_epi16(q4bits_2, 4)), _mm_set1_epi8(8));

        const __m128i p16_1_0 = mul_add_epi8_sse(q4b_1_0, q8b_1_0);
        const __m128i p16_1_1 = mul_add_epi8_sse(q4b_1_1, q8b_1_1);
        const __m128i p16_2_0 = mul_add_epi8_sse(q4b_2_0, q8b_2_0);
        const __m128i p16_2_1 = mul_add_epi8_sse(q4b_2_1, q8b_2_1);
        const __m128i p_1 = _mm_add_epi16(p16_1_0, p16_1_1);
        const __m128i p_2 = _mm_add_epi16(p16_2_0, p16_2_1);
        const __m256 p = sum_i16_pairs_float(p_2, p_1);

        const __m256 deltas = quad_fp16_delta_float(x[ib].d, y[ib].d, x[ib + 1].d, y[ib + 1].d);
        accum = _mm256_add_ps(_mm256_mul_ps(deltas, p), accum);
    }

    sumf = hsum_float_8(accum);
#elif defined(__SSSE3__)
    // set constants
    const __m128i lowMask = _mm_set1_epi8(0xF);
    const __m128i off = _mm_set1_epi8(8);

    // Initialize accumulator with zeros
    __m128 acc_0 = _mm_setzero_ps();
    __m128 acc_1 = _mm_setzero_ps();
    __m128 acc_2 = _mm_setzero_ps();
    __m128 acc_3 = _mm_setzero_ps();

    for (; ib + 1 < nb; ib += 2) {
        _mm_prefetch(&x[ib] + sizeof(block_q4_0), _MM_HINT_T0);
        _mm_prefetch(&y[ib] + sizeof(block_q8_0), _MM_HINT_T0);

        // Compute combined scale for the block 0 and 1
        const __m128 d_0_1 = _mm_set1_ps(AZ_FP16_TO_FP32(x[ib].d) * AZ_FP16_TO_FP32(y[ib].d));

        const __m128i tmp_0_1 = _mm_loadu_si128((const __m128i *)x[ib].qs);

        __m128i bx_0 = _mm_and_si128(lowMask, tmp_0_1);
        __m128i by_0 = _mm_loadu_si128((const __m128i *)y[ib].qs);
        bx_0 = _mm_sub_epi8(bx_0, off);
        const __m128i i32_0 = mul_sum_i8_pairs(bx_0, by_0);

        __m128i bx_1 = _mm_and_si128(lowMask, _mm_srli_epi64(tmp_0_1, 4));
        __m128i by_1 = _mm_loadu_si128((const __m128i *)(y[ib].qs + 16));
        bx_1 = _mm_sub_epi8(bx_1, off);
        const __m128i i32_1 = mul_sum_i8_pairs(bx_1, by_1);

        _mm_prefetch(&x[ib] + 2 * sizeof(block_q4_0), _MM_HINT_T0);
        _mm_prefetch(&y[ib] + 2 * sizeof(block_q8_0), _MM_HINT_T0);

        // Compute combined scale for the block 2 and 3
        const __m128 d_2_3 = _mm_set1_ps(AZ_FP16_TO_FP32(x[ib + 1].d) * AZ_FP16_TO_FP32(y[ib + 1].d));

        const __m128i tmp_2_3 = _mm_loadu_si128((const __m128i *)x[ib + 1].qs);

        __m128i bx_2 = _mm_and_si128(lowMask, tmp_2_3);
        __m128i by_2 = _mm_loadu_si128((const __m128i *)y[ib + 1].qs);
        bx_2 = _mm_sub_epi8(bx_2, off);
        const __m128i i32_2 = mul_sum_i8_pairs(bx_2, by_2);

        __m128i bx_3 = _mm_and_si128(lowMask, _mm_srli_epi64(tmp_2_3, 4));
        __m128i by_3 = _mm_loadu_si128((const __m128i *)(y[ib + 1].qs + 16));
        bx_3 = _mm_sub_epi8(bx_3, off);
        const __m128i i32_3 = mul_sum_i8_pairs(bx_3, by_3);

        // Convert int32_t to float
        __m128 p0 = _mm_cvtepi32_ps(i32_0);
        __m128 p1 = _mm_cvtepi32_ps(i32_1);
        __m128 p2 = _mm_cvtepi32_ps(i32_2);
        __m128 p3 = _mm_cvtepi32_ps(i32_3);

        // Apply the scale
        __m128 p0_d = _mm_mul_ps(d_0_1, p0);
        __m128 p1_d = _mm_mul_ps(d_0_1, p1);
        __m128 p2_d = _mm_mul_ps(d_2_3, p2);
        __m128 p3_d = _mm_mul_ps(d_2_3, p3);

        // Acummulate
        acc_0 = _mm_add_ps(p0_d, acc_0);
        acc_1 = _mm_add_ps(p1_d, acc_1);
        acc_2 = _mm_add_ps(p2_d, acc_2);
        acc_3 = _mm_add_ps(p3_d, acc_3);
    }

    sumf = hsum_float_4x4(acc_0, acc_1, acc_2, acc_3);
#endif
    for (; ib < nb; ++ib) {
        int sumi0 = 0;
        int sumi1 = 0;

        for (size_t j = 0; j < qk / 2; ++j) {
            const int v0 = (x[ib].qs[j] & 0x0F) - 8;
            const int v1 = (x[ib].qs[j] >> 4) - 8;

            sumi0 += (v0 * y[ib].qs[j]);
            sumi1 += (v1 * y[ib].qs[j + qk / 2]);
        }

        int sumi = sumi0 + sumi1;
        sumf += sumi * AZ_FP16_TO_FP32(x[ib].d) * AZ_FP16_TO_FP32(y[ib].d);
    }

    return sumf;
}
}  // namespace az::cpu
