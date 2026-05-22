#pragma once

#include <cstdint>
#include <cstring>

#include <sys/prctl.h>

#include "powerinfer-cpu-data.hpp"
#include "powerinfer-log.hpp"

#if defined(__ARM_FEATURE_SVE)
static int sve_cnt = PR_SVE_VL_LEN_MASK & prctl(PR_SVE_GET_VL);
#endif

using vec_dot_func_t = void (*)(int n, float * s, const void * vx, const void * vy);

// Ref:llama.cpp(https://github.com/ggml-org/llama.cpp) ggml/src/ggml-cpu/arch/x86/quants.c
#define MM256_SET_M128I(a, b) _mm256_insertf128_si256(_mm256_castsi128_si256(b), (a), 1)

#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__) || defined(__SSSE3__)
// multiply int8_t, add results pairwise twice
static inline __m128i mul_sum_i8_pairs(const __m128i x, const __m128i y) {
    // Get absolute values of x vectors
    const __m128i ax = _mm_sign_epi8(x, x);
    // Sign the values of the y vectors
    const __m128i sy = _mm_sign_epi8(y, x);
    // Perform multiplication and create 16-bit values
    const __m128i dot = _mm_maddubs_epi16(ax, sy);
    const __m128i ones = _mm_set1_epi16(1);
    return _mm_madd_epi16(ones, dot);
}

#if __AVX__ || __AVX2__ || __AVX512F__
// horizontally add 8 floats
static inline float hsum_float_8(const __m256 x) {
    __m128 res = _mm256_extractf128_ps(x, 1);
    res = _mm_add_ps(res, _mm256_castps256_ps128(x));
    res = _mm_add_ps(res, _mm_movehl_ps(res, res));
    res = _mm_add_ss(res, _mm_movehdup_ps(res));
    return _mm_cvtss_f32(res);
}

// horizontally add 8 int32_t
static inline int hsum_i32_8(const __m256i a) {
    const __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(a), _mm256_extractf128_si256(a, 1));
    const __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
    const __m128i sum64 = _mm_add_epi32(hi64, sum128);
    const __m128i hi32  = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_cvtsi128_si32(_mm_add_epi32(sum64, hi32));
}

// horizontally add 4 int32_t
static inline int hsum_i32_4(const __m128i a) {
    const __m128i hi64 = _mm_unpackhi_epi64(a, a);
    const __m128i sum64 = _mm_add_epi32(hi64, a);
    const __m128i hi32  = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_cvtsi128_si32(_mm_add_epi32(sum64, hi32));
}

#if defined(__AVX2__) || defined(__AVX512F__)
// spread 32 bits to 32 bytes { 0x00, 0xFF }
static inline __m256i bytes_from_bits_32(const uint8_t * x) {
    uint32_t x32;
    memcpy(&x32, x, sizeof(uint32_t));
    const __m256i shuf_mask = _mm256_set_epi64x(
            0x0303030303030303, 0x0202020202020202,
            0x0101010101010101, 0x0000000000000000);
    __m256i bytes = _mm256_shuffle_epi8(_mm256_set1_epi32(x32), shuf_mask);
    const __m256i bit_mask = _mm256_set1_epi64x(0x7fbfdfeff7fbfdfe);
    bytes = _mm256_or_si256(bytes, bit_mask);
    return _mm256_cmpeq_epi8(bytes, _mm256_set1_epi64x(-1));
}

// Unpack 32 4-bit fields into 32 bytes
// The output vector contains 32 bytes, each one in [ 0 .. 15 ] interval
static inline __m256i bytes_from_nibbles_32(const uint8_t * rsi)
{
    const __m128i tmp = _mm_loadu_si128((const __m128i *)rsi);
    const __m256i bytes = MM256_SET_M128I(_mm_srli_epi16(tmp, 4), tmp);
    const __m256i lowMask = _mm256_set1_epi8( 0xF );
    return _mm256_and_si256(lowMask, bytes);
}

// add int16_t pairwise and return as float vector
static inline __m256 sum_i16_pairs_float(const __m256i x) {
    const __m256i ones = _mm256_set1_epi16(1);
    const __m256i summed_pairs = _mm256_madd_epi16(ones, x);
    return _mm256_cvtepi32_ps(summed_pairs);
}

static inline __m256 mul_sum_us8_pairs_float(const __m256i ax, const __m256i sy) {
#if defined(__AVX512VNNI__) && defined(__AVX512VL__)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i summed_pairs = _mm256_dpbusd_epi32(zero, ax, sy);
    return _mm256_cvtepi32_ps(summed_pairs);
#elif defined(__AVXVNNI__)
    const __m256i zero = _mm256_setzero_si256();
    const __m256i summed_pairs = _mm256_dpbusd_avx_epi32(zero, ax, sy);
    return _mm256_cvtepi32_ps(summed_pairs);
#else
    // Perform multiplication and create 16-bit values
    const __m256i dot = _mm256_maddubs_epi16(ax, sy);
    return sum_i16_pairs_float(dot);
#endif
}

// multiply int8_t, add results pairwise twice and return as float vector
static inline __m256 mul_sum_i8_pairs_float(const __m256i x, const __m256i y) {
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

static inline __m128i packNibbles( __m256i bytes )
{
    // Move bits within 16-bit lanes from 0000_abcd_0000_efgh into 0000_0000_abcd_efgh
#if __AVX512F__
    const __m256i bytes_srli_4 = _mm256_srli_epi16(bytes, 4);   // 0000_0000_abcd_0000
    bytes = _mm256_or_si256(bytes, bytes_srli_4);               // 0000_abcd_abcd_efgh
    return _mm256_cvtepi16_epi8(bytes);                         // abcd_efgh
#else
    const __m256i lowByte = _mm256_set1_epi16( 0xFF );
    __m256i high = _mm256_andnot_si256( lowByte, bytes );
    __m256i low = _mm256_and_si256( lowByte, bytes );
    high = _mm256_srli_epi16( high, 4 );
    bytes = _mm256_or_si256( low, high );

    // Compress uint16_t lanes into bytes
    __m128i r0 = _mm256_castsi256_si128( bytes );
    __m128i r1 = _mm256_extracti128_si256( bytes, 1 );
    return _mm_packus_epi16( r0, r1 );
#endif
}
#elif defined(__AVX__)
static inline __m128i packNibbles( __m128i bytes1, __m128i bytes2 )
{
    // Move bits within 16-bit lanes from 0000_abcd_0000_efgh into 0000_0000_abcd_efgh
    const __m128i lowByte = _mm_set1_epi16( 0xFF );
    __m128i high = _mm_andnot_si128( lowByte, bytes1 );
    __m128i low = _mm_and_si128( lowByte, bytes1 );
    high = _mm_srli_epi16( high, 4 );
    bytes1 = _mm_or_si128( low, high );
    high = _mm_andnot_si128( lowByte, bytes2 );
    low = _mm_and_si128( lowByte, bytes2 );
    high = _mm_srli_epi16( high, 4 );
    bytes2 = _mm_or_si128( low, high );

    return _mm_packus_epi16( bytes1, bytes2);
}

static inline __m128i mul_add_epi8_sse(const __m128i x, const __m128i y) {
    const __m128i ax = _mm_sign_epi8(x, x);
    const __m128i sy = _mm_sign_epi8(y, x);
    return _mm_maddubs_epi16(ax, sy);
}

// spread 32 bits to 32 bytes { 0x00, 0xFF }
static inline __m256i bytes_from_bits_32(const uint8_t * x) {
    uint32_t x32;
    memcpy(&x32, x, sizeof(uint32_t));
    const __m128i shuf_maskl = _mm_set_epi64x(0x0101010101010101, 0x0000000000000000);
    const __m128i shuf_maskh = _mm_set_epi64x(0x0303030303030303, 0x0202020202020202);
    __m128i bytesl = _mm_shuffle_epi8(_mm_set1_epi32(x32), shuf_maskl);
    __m128i bytesh = _mm_shuffle_epi8(_mm_set1_epi32(x32), shuf_maskh);
    const __m128i bit_mask = _mm_set1_epi64x(0x7fbfdfeff7fbfdfe);
    bytesl = _mm_or_si128(bytesl, bit_mask);
    bytesh = _mm_or_si128(bytesh, bit_mask);
    bytesl = _mm_cmpeq_epi8(bytesl, _mm_set1_epi64x(-1));
    bytesh = _mm_cmpeq_epi8(bytesh, _mm_set1_epi64x(-1));
    return MM256_SET_M128I(bytesh, bytesl);
}

// Unpack 32 4-bit fields into 32 bytes
// The output vector contains 32 bytes, each one in [ 0 .. 15 ] interval
static inline __m256i bytes_from_nibbles_32(const uint8_t * rsi)
{
    // Load 16 bytes from memory
    __m128i tmpl = _mm_loadu_si128((const __m128i *)rsi);
    __m128i tmph = _mm_srli_epi16(tmpl, 4);
    const __m128i lowMask = _mm_set1_epi8(0xF);
    tmpl = _mm_and_si128(lowMask, tmpl);
    tmph = _mm_and_si128(lowMask, tmph);
    return MM256_SET_M128I(tmph, tmpl);
}

// add int16_t pairwise and return as float vector
static inline __m256 sum_i16_pairs_float(const __m128i xh, const __m128i xl) {
    const __m128i ones = _mm_set1_epi16(1);
    const __m128i summed_pairsl = _mm_madd_epi16(ones, xl);
    const __m128i summed_pairsh = _mm_madd_epi16(ones, xh);
    const __m256i summed_pairs = MM256_SET_M128I(summed_pairsh, summed_pairsl);
    return _mm256_cvtepi32_ps(summed_pairs);
}

static inline __m256 mul_sum_us8_pairs_float(const __m256i ax, const __m256i sy) {
    const __m128i axl = _mm256_castsi256_si128(ax);
    const __m128i axh = _mm256_extractf128_si256(ax, 1);
    const __m128i syl = _mm256_castsi256_si128(sy);
    const __m128i syh = _mm256_extractf128_si256(sy, 1);
    // Perform multiplication and create 16-bit values
    const __m128i dotl = _mm_maddubs_epi16(axl, syl);
    const __m128i doth = _mm_maddubs_epi16(axh, syh);
    return sum_i16_pairs_float(doth, dotl);
}

// multiply int8_t, add results pairwise twice and return as float vector
static inline __m256 mul_sum_i8_pairs_float(const __m256i x, const __m256i y) {
    const __m128i xl = _mm256_castsi256_si128(x);
    const __m128i xh = _mm256_extractf128_si256(x, 1);
    const __m128i yl = _mm256_castsi256_si128(y);
    const __m128i yh = _mm256_extractf128_si256(y, 1);
    // Get absolute values of x vectors
    const __m128i axl = _mm_sign_epi8(xl, xl);
    const __m128i axh = _mm_sign_epi8(xh, xh);
    // Sign the values of the y vectors
    const __m128i syl = _mm_sign_epi8(yl, xl);
    const __m128i syh = _mm_sign_epi8(yh, xh);
    // Perform multiplication and create 16-bit values
    const __m128i dotl = _mm_maddubs_epi16(axl, syl);
    const __m128i doth = _mm_maddubs_epi16(axh, syh);
    return sum_i16_pairs_float(doth, dotl);
}

// larger version of mul_sum_i8_pairs_float where x and y are each represented by four 128-bit vectors
static inline __m256 mul_sum_i8_quad_float(const __m128i x_1_0, const __m128i x_1_1, const __m128i x_2_0, const __m128i x_2_1,
                                           const __m128i y_1_0, const __m128i y_1_1, const __m128i y_2_0, const __m128i y_2_1) {
    const __m128i mone = _mm_set1_epi16(1);

    const __m128i p16_1_0 = mul_add_epi8_sse(x_1_0, y_1_0);
    const __m128i p16_1_1 = mul_add_epi8_sse(x_1_1, y_1_1);
    const __m128i p16_2_0 = mul_add_epi8_sse(x_2_0, y_2_0);
    const __m128i p16_2_1 = mul_add_epi8_sse(x_2_1, y_2_1);
    const __m128i p_1_0 = _mm_madd_epi16(p16_1_0, mone);
    const __m128i p_1_1 = _mm_madd_epi16(p16_1_1, mone);
    const __m128i p_2_0 = _mm_madd_epi16(p16_2_0, mone);
    const __m128i p_2_1 = _mm_madd_epi16(p16_2_1, mone);
    const __m128i p_1 = _mm_add_epi32(p_1_0, p_1_1);
    const __m128i p_2 = _mm_add_epi32(p_2_0, p_2_1);
    return _mm256_cvtepi32_ps(MM256_SET_M128I(p_2, p_1));
}

// quad fp16 delta calculation
static inline __m256 quad_fp16_delta_float(const float x0, const float y0, const float x1, const float y1) {
    // GGML_CPU_FP16_TO_FP32 is faster than Intel F16C
    return _mm256_set_m128(_mm_set1_ps(GGML_CPU_FP16_TO_FP32(x1) * GGML_CPU_FP16_TO_FP32(y1)),
                           _mm_set1_ps(GGML_CPU_FP16_TO_FP32(x0) * GGML_CPU_FP16_TO_FP32(y0)));
}
#endif
#elif defined(__SSSE3__)
// horizontally add 4x4 floats
static inline float hsum_float_4x4(const __m128 a, const __m128 b, const __m128 c, const __m128 d) {
    __m128 res_0 =_mm_hadd_ps(a, b);
    __m128 res_1 =_mm_hadd_ps(c, d);
    __m128 res =_mm_hadd_ps(res_0, res_1);
    res =_mm_hadd_ps(res, res);
    res =_mm_hadd_ps(res, res);

    return _mm_cvtss_f32(res);
}
#endif // __AVX__ || __AVX2__ || __AVX512F__
#endif // defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__) || defined(__SSSE3__)


#if defined (__ARM_NEON)

    #ifdef _MSC_VER

    typedef uint16_t ggml_fp16_internal_t;

    #define ggml_vld1q_u32(w,x,y,z) { ((w) + ((uint64_t)(x) << 32)), ((y) + ((uint64_t)(z) << 32)) }

    #else

    typedef __fp16 ggml_fp16_internal_t;

    #define ggml_vld1q_u32(w,x,y,z) { (w), (x), (y), (z) }

    #endif // _MSC_VER

    #if !defined(__aarch64__)

    // 32-bit ARM compatibility

    // vaddlvq_s16
    // vpaddq_s16
    // vpaddq_s32
    // vaddvq_s32
    // vaddvq_f32
    // vmaxvq_f32
    // vcvtnq_s32_f32
    // vzip1_u8
    // vzip2_u8

    inline static int32_t vaddlvq_s16(int16x8_t v) {
        int32x4_t v0 = vreinterpretq_s32_s64(vpaddlq_s32(vpaddlq_s16(v)));
        return vgetq_lane_s32(v0, 0) + vgetq_lane_s32(v0, 2);
    }

    inline static int16x8_t vpaddq_s16(int16x8_t a, int16x8_t b) {
        int16x4_t a0 = vpadd_s16(vget_low_s16(a), vget_high_s16(a));
        int16x4_t b0 = vpadd_s16(vget_low_s16(b), vget_high_s16(b));
        return vcombine_s16(a0, b0);
    }

    inline static int32x4_t vpaddq_s32(int32x4_t a, int32x4_t b) {
        int32x2_t a0 = vpadd_s32(vget_low_s32(a), vget_high_s32(a));
        int32x2_t b0 = vpadd_s32(vget_low_s32(b), vget_high_s32(b));
        return vcombine_s32(a0, b0);
    }

    inline static int32_t vaddvq_s32(int32x4_t v) {
        return vgetq_lane_s32(v, 0) + vgetq_lane_s32(v, 1) + vgetq_lane_s32(v, 2) + vgetq_lane_s32(v, 3);
    }

    inline static float vaddvq_f32(float32x4_t v) {
        return vgetq_lane_f32(v, 0) + vgetq_lane_f32(v, 1) + vgetq_lane_f32(v, 2) + vgetq_lane_f32(v, 3);
    }

    inline static float vmaxvq_f32(float32x4_t v) {
        return
            MAX(MAX(vgetq_lane_f32(v, 0), vgetq_lane_f32(v, 1)),
                MAX(vgetq_lane_f32(v, 2), vgetq_lane_f32(v, 3)));
    }

    inline static int32x4_t vcvtnq_s32_f32(float32x4_t v) {
        int32x4_t res;

        res[0] = roundf(vgetq_lane_f32(v, 0));
        res[1] = roundf(vgetq_lane_f32(v, 1));
        res[2] = roundf(vgetq_lane_f32(v, 2));
        res[3] = roundf(vgetq_lane_f32(v, 3));

        return res;
    }

    inline static uint8x8_t vzip1_u8(uint8x8_t a, uint8x8_t b) {
        uint8x8_t res;

        res[0] = a[0]; res[1] = b[0];
        res[2] = a[1]; res[3] = b[1];
        res[4] = a[2]; res[5] = b[2];
        res[6] = a[3]; res[7] = b[3];

        return res;
    }

    inline static uint8x8_t vzip2_u8(uint8x8_t a, uint8x8_t b) {
        uint8x8_t res;

        res[0] = a[4]; res[1] = b[4];
        res[2] = a[5]; res[3] = b[5];
        res[4] = a[6]; res[5] = b[6];
        res[6] = a[7]; res[7] = b[7];

        return res;
    }

    // vld1q_s16_x2
    // vld1q_u8_x2
    // vld1q_u8_x4
    // vld1q_s8_x2
    // vld1q_s8_x4
    // TODO: double-check these work correctly

    typedef struct ggml_int16x8x2_t {
        int16x8_t val[2];
    } ggml_int16x8x2_t;

    inline static ggml_int16x8x2_t ggml_vld1q_s16_x2(const int16_t * ptr) {
        ggml_int16x8x2_t res;

        res.val[0] = vld1q_s16(ptr + 0);
        res.val[1] = vld1q_s16(ptr + 8);

        return res;
    }

    typedef struct ggml_uint8x16x2_t {
        uint8x16_t val[2];
    } ggml_uint8x16x2_t;

    inline static ggml_uint8x16x2_t ggml_vld1q_u8_x2(const uint8_t * ptr) {
        ggml_uint8x16x2_t res;

        res.val[0] = vld1q_u8(ptr + 0);
        res.val[1] = vld1q_u8(ptr + 16);

        return res;
    }

    typedef struct ggml_uint8x16x4_t {
        uint8x16_t val[4];
    } ggml_uint8x16x4_t;

    inline static ggml_uint8x16x4_t ggml_vld1q_u8_x4(const uint8_t * ptr) {
        ggml_uint8x16x4_t res;

        res.val[0] = vld1q_u8(ptr + 0);
        res.val[1] = vld1q_u8(ptr + 16);
        res.val[2] = vld1q_u8(ptr + 32);
        res.val[3] = vld1q_u8(ptr + 48);

        return res;
    }

    typedef struct ggml_int8x16x2_t {
        int8x16_t val[2];
    } ggml_int8x16x2_t;

    inline static ggml_int8x16x2_t ggml_vld1q_s8_x2(const int8_t * ptr) {
        ggml_int8x16x2_t res;

        res.val[0] = vld1q_s8(ptr + 0);
        res.val[1] = vld1q_s8(ptr + 16);

        return res;
    }

    typedef struct ggml_int8x16x4_t {
        int8x16_t val[4];
    } ggml_int8x16x4_t;

    inline static ggml_int8x16x4_t ggml_vld1q_s8_x4(const int8_t * ptr) {
        ggml_int8x16x4_t res;

        res.val[0] = vld1q_s8(ptr + 0);
        res.val[1] = vld1q_s8(ptr + 16);
        res.val[2] = vld1q_s8(ptr + 32);
        res.val[3] = vld1q_s8(ptr + 48);

        return res;
    }

    // NOTE: not tested
    inline static int8x16_t ggml_vqtbl1q_s8(int8x16_t a, uint8x16_t b) {
        int8x16_t res;

        res[ 0] = a[b[ 0]];
        res[ 1] = a[b[ 1]];
        res[ 2] = a[b[ 2]];
        res[ 3] = a[b[ 3]];
        res[ 4] = a[b[ 4]];
        res[ 5] = a[b[ 5]];
        res[ 6] = a[b[ 6]];
        res[ 7] = a[b[ 7]];
        res[ 8] = a[b[ 8]];
        res[ 9] = a[b[ 9]];
        res[10] = a[b[10]];
        res[11] = a[b[11]];
        res[12] = a[b[12]];
        res[13] = a[b[13]];
        res[14] = a[b[14]];
        res[15] = a[b[15]];

        return res;
    }

    // NOTE: not tested
    inline static uint8x16_t ggml_vqtbl1q_u8(uint8x16_t a, uint8x16_t b) {
        uint8x16_t res;

        res[ 0] = a[b[ 0]];
        res[ 1] = a[b[ 1]];
        res[ 2] = a[b[ 2]];
        res[ 3] = a[b[ 3]];
        res[ 4] = a[b[ 4]];
        res[ 5] = a[b[ 5]];
        res[ 6] = a[b[ 6]];
        res[ 7] = a[b[ 7]];
        res[ 8] = a[b[ 8]];
        res[ 9] = a[b[ 9]];
        res[10] = a[b[10]];
        res[11] = a[b[11]];
        res[12] = a[b[12]];
        res[13] = a[b[13]];
        res[14] = a[b[14]];
        res[15] = a[b[15]];

        return res;
    }

    #else

    #define ggml_int16x8x2_t  int16x8x2_t
    #define ggml_uint8x16x2_t uint8x16x2_t
    #define ggml_uint8x16x4_t uint8x16x4_t
    #define ggml_int8x16x2_t  int8x16x2_t
    #define ggml_int8x16x4_t  int8x16x4_t

    #define ggml_vld1q_s16_x2 vld1q_s16_x2
    #define ggml_vld1q_u8_x2  vld1q_u8_x2
    #define ggml_vld1q_u8_x4  vld1q_u8_x4
    #define ggml_vld1q_s8_x2  vld1q_s8_x2
    #define ggml_vld1q_s8_x4  vld1q_s8_x4
    #define ggml_vqtbl1q_s8   vqtbl1q_s8
    #define ggml_vqtbl1q_u8   vqtbl1q_u8

    #endif // !defined(__aarch64__)

    #if !defined(__ARM_FEATURE_DOTPROD)

    inline static int32x4_t ggml_vdotq_s32(int32x4_t acc, int8x16_t a, int8x16_t b) {
        const int16x8_t p0 = vmull_s8(vget_low_s8 (a), vget_low_s8 (b));
        const int16x8_t p1 = vmull_s8(vget_high_s8(a), vget_high_s8(b));

        return vaddq_s32(acc, vaddq_s32(vpaddlq_s16(p0), vpaddlq_s16(p1)));
    }

    #else

    #define ggml_vdotq_s32(a, b, c) vdotq_s32(a, b, c)

    #endif // !defined(__ARM_FEATURE_DOTPROD)

#endif // __ARM_NEON

inline void ggml_vec_dot_q4_0_q8_0(int n, float *__restrict__ s, const void *__restrict__ vx, const void *__restrict__ vy) {
    const int qk = QK8_0;
    const int nb = n / qk;
    POWERINFER_ASSERT(qk * nb == n);

    const block_q4_0 *__restrict__ x = static_cast<const block_q4_0 *>(vx);
    const block_q8_0 *__restrict__ y = static_cast<const block_q8_0 *>(vy);

    int ib = 0;
    float sumf = 0;

#if defined(__ARM_FEATURE_SVE)
    svfloat32_t sumv0 = svdup_n_f32(0.0f);
    svfloat32_t sumv1 = svdup_n_f32(0.0f);

    const int vector_length = sve_cnt*8;

    // VLA Implementation using switch case
    switch (vector_length) {
        case 128:
            {
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
                    sumv0 = svmla_n_f32_x(ph4, sumv0, svcvt_f32_s32_x(ph4, svadd_x(ph4,
                                    svdot_s32(svdup_n_s32(0), qx0ls, qy0l),
                                    svdot_s32(svdup_n_s32(0), qx0hs, qy0h))), POWERINFER_FP16_TO_FP32(x0->d)*POWERINFER_FP16_TO_FP32(y0->d));
                    sumv1 = svmla_n_f32_x(ph4, sumv1, svcvt_f32_s32_x(ph4, svadd_x(ph4,
                                    svdot_s32(svdup_n_s32(0), qx1ls, qy1l),
                                    svdot_s32(svdup_n_s32(0), qx1hs, qy1h))), POWERINFER_FP16_TO_FP32(x1->d)*POWERINFER_FP16_TO_FP32(y1->d));
                }

                sumf = svaddv_f32(svptrue_b32(), svadd_f32_x(svptrue_b32(), sumv0, sumv1));
            } break;
        case 256:
            {
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
                    sumv0 = svmla_n_f32_x(svptrue_b32(), sumv0, svcvt_f32_s32_x(svptrue_b32(),
                                svdot_s32(svdup_n_s32(0), qx0s, qy0)), POWERINFER_FP16_TO_FP32(x0->d)*POWERINFER_FP16_TO_FP32(y0->d));
                    sumv1 = svmla_n_f32_x(svptrue_b32(), sumv1, svcvt_f32_s32_x(svptrue_b32(),
                                svdot_s32(svdup_n_s32(0), qx1s, qy1)), POWERINFER_FP16_TO_FP32(x1->d)*POWERINFER_FP16_TO_FP32(y1->d));
                }

                sumf = svaddv_f32(svptrue_b32(), svadd_f32_x(svptrue_b32(), sumv0, sumv1));
            } break;
        case 512:
            {
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
                    sumv0 = svmla_n_f32_x(ph32, sumv0, svcvt_f32_s32_x(ph32,
                                svdot_s32(svdup_n_s32(0), qx0s, qy0)), POWERINFER_FP16_TO_FP32(x0->d)*POWERINFER_FP16_TO_FP32(y0->d));
                    sumv1 = svmla_n_f32_x(ph32, sumv1, svcvt_f32_s32_x(ph32,
                                svdot_s32(svdup_n_s32(0), qx1s, qy1)), POWERINFER_FP16_TO_FP32(x1->d)*POWERINFER_FP16_TO_FP32(y1->d));
                }

                sumf = svaddv_f32(ph32, svadd_f32_x(ph32, sumv0, sumv1));
            } break;
        default:
            POWERINFER_ASSERT(false && "Unsupported vector length");
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
        const int8x16_t  s8b = vdupq_n_s8(0x8);

        const uint8x16_t v0_0 = vld1q_u8(x0->qs);
        const uint8x16_t v0_1 = vld1q_u8(x1->qs);

        // 4-bit -> 8-bit
        const int8x16_t v0_0l = vreinterpretq_s8_u8(vandq_u8  (v0_0, m4b));
        const int8x16_t v0_0h = vreinterpretq_s8_u8(vshrq_n_u8(v0_0, 4));
        const int8x16_t v0_1l = vreinterpretq_s8_u8(vandq_u8  (v0_1, m4b));
        const int8x16_t v0_1h = vreinterpretq_s8_u8(vshrq_n_u8(v0_1, 4));

        // sub 8
        const int8x16_t v0_0ls = vsubq_s8(v0_0l, s8b);
        const int8x16_t v0_0hs = vsubq_s8(v0_0h, s8b);
        const int8x16_t v0_1ls = vsubq_s8(v0_1l, s8b);
        const int8x16_t v0_1hs = vsubq_s8(v0_1h, s8b);

        // load y
        const int8x16_t v1_0l = vld1q_s8(y0->qs);
        const int8x16_t v1_0h = vld1q_s8(y0->qs + 16);
        const int8x16_t v1_1l = vld1q_s8(y1->qs);
        const int8x16_t v1_1h = vld1q_s8(y1->qs + 16);

        // dot product into int32x4_t
        const int32x4_t p_0 = ggml_vdotq_s32(ggml_vdotq_s32(vdupq_n_s32(0), v0_0ls, v1_0l), v0_0hs, v1_0h);
        const int32x4_t p_1 = ggml_vdotq_s32(ggml_vdotq_s32(vdupq_n_s32(0), v0_1ls, v1_1l), v0_1hs, v1_1h);

        sumv0 = vmlaq_n_f32(sumv0, vcvtq_f32_s32(p_0), POWERINFER_FP16_TO_FP32(x0->d)*POWERINFER_FP16_TO_FP32(y0->d));
        sumv1 = vmlaq_n_f32(sumv1, vcvtq_f32_s32(p_1), POWERINFER_FP16_TO_FP32(x1->d)*POWERINFER_FP16_TO_FP32(y1->d));
    }

    sumf = vaddvq_f32(sumv0) + vaddvq_f32(sumv1);
#elif defined(__AVX2__)
    // Initialize accumulator with zeros
    __m256 acc = _mm256_setzero_ps();

    // Main loop
    for (; ib < nb; ++ib) {
        /* Compute combined scale for the block */
        const __m256 d = _mm256_set1_ps( POWERINFER_FP16_TO_FP32(x[ib].d) * POWERINFER_FP16_TO_FP32(y[ib].d) );

        __m256i qx = bytes_from_nibbles_32(x[ib].qs);

        // Now we have a vector with bytes in [ 0 .. 15 ] interval. Offset them into [ -8 .. +7 ] interval.
        const __m256i off = _mm256_set1_epi8( 8 );
        qx = _mm256_sub_epi8( qx, off );

        __m256i qy = _mm256_loadu_si256((const __m256i *)y[ib].qs);

        const __m256 q = mul_sum_i8_pairs_float(qx, qy);

        /* Multiply q with scale and accumulate */
        acc = _mm256_fmadd_ps( d, q, acc );
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
        const __m128i q4b_1_1 = _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), _mm_srli_epi16(q4bits_1, 4)), _mm_set1_epi8(8));
        const __m128i q4b_2_0 = _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), q4bits_2), _mm_set1_epi8(8));
        const __m128i q4b_2_1 = _mm_sub_epi8(_mm_and_si128(_mm_set1_epi8(15), _mm_srli_epi16(q4bits_2, 4)), _mm_set1_epi8(8));

        const __m128i p16_1_0 = mul_add_epi8_sse(q4b_1_0, q8b_1_0);
        const __m128i p16_1_1 = mul_add_epi8_sse(q4b_1_1, q8b_1_1);
        const __m128i p16_2_0 = mul_add_epi8_sse(q4b_2_0, q8b_2_0);
        const __m128i p16_2_1 = mul_add_epi8_sse(q4b_2_1, q8b_2_1);
        const __m128i p_1 = _mm_add_epi16(p16_1_0, p16_1_1);
        const __m128i p_2 = _mm_add_epi16(p16_2_0, p16_2_1);
        const __m256 p =  sum_i16_pairs_float(p_2, p_1);

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
        const __m128 d_0_1 = _mm_set1_ps( POWERINFER_FP16_TO_FP32(x[ib].d) * POWERINFER_FP16_TO_FP32(y[ib].d) );

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
        const __m128 d_2_3 = _mm_set1_ps( POWERINFER_FP16_TO_FP32(x[ib + 1].d) * POWERINFER_FP16_TO_FP32(y[ib + 1].d) );

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
        __m128 p0_d = _mm_mul_ps( d_0_1, p0 );
        __m128 p1_d = _mm_mul_ps( d_0_1, p1 );
        __m128 p2_d = _mm_mul_ps( d_2_3, p2 );
        __m128 p3_d = _mm_mul_ps( d_2_3, p3 );

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

        for (int j = 0; j < qk/2; ++j) {
            const int v0 = (x[ib].qs[j] & 0x0F) - 8;
            const int v1 = (x[ib].qs[j] >>   4) - 8;

            sumi0 += (v0 * y[ib].qs[j]);
            sumi1 += (v1 * y[ib].qs[j + qk/2]);
        }

        int sumi = sumi0 + sumi1;
        sumf += sumi*POWERINFER_FP16_TO_FP32(x[ib].d)*POWERINFER_FP16_TO_FP32(y[ib].d);
    }

    *s = sumf;
}
