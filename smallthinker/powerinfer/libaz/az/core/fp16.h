/**
 * Reference: ggml-impl.h
 */

#pragma once

#include "az/core/intrinsics.hpp"
#include "stdint.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct az_fp16 {
#if defined(__ARM_NEON)
    __fp16 value;
#else
    uint16_t value;
#endif
};

typedef struct az_fp16 az_fp16_t;

// __FMA__ and __F16C__ are not defined in MSVC, however they are implied with AVX2/AVX512
#if defined(_MSC_VER) && (defined(__AVX2__) || defined(__AVX512F__))
#ifndef __FMA__
#define __FMA__
#endif
#ifndef __F16C__
#define __F16C__
#endif
#ifndef __SSE3__
#define __SSE3__
#endif
#endif

// 16-bit float
// on Arm, we use __fp16
// on x86, we use uint16_t
#if defined(__ARM_NEON)

#define WRAP_FP16(x) ((az_fp16_t){.value = (x)})

#define AZ_COMPUTE_FP16_TO_FP32(x) ((float)((x).value))
#define AZ_COMPUTE_FP32_TO_FP16(x) WRAP_FP16((__fp16)(x))

#define AZ_FP16_TO_FP32(x) ((float)((x).value))
#define AZ_FP32_TO_FP16(x) WRAP_FP16((__fp16)(x))

#else

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
#else
#ifdef __POWER9_VECTOR__
#include <altivec.h>
#undef bool
#define bool _Bool
#else
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <intrin.h>
#else
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__) || defined(__SSSE3__) || defined(__SSE3__)
#if !defined(__riscv)
#include <immintrin.h>
#endif
#endif
#endif
#endif
#endif

#ifdef __riscv_v_intrinsic
#include <riscv_vector.h>
#endif

#ifdef __F16C__

#ifdef _MSC_VER
#define AZ_COMPUTE_FP16_TO_FP32(x) _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128((x).value)))
#define AZ_COMPUTE_FP32_TO_FP16(x) ((az_fp16_t){.value = _mm_extract_epi16(_mm_cvtps_ph(_mm_set_ss((x), 0), 0)})
#else
#define AZ_COMPUTE_FP16_TO_FP32(x) _cvtsh_ss((x).value)
#define AZ_COMPUTE_FP32_TO_FP16(x) ((az_fp16_t){.value = _cvtss_sh((x), 0)})
#endif

#else

// FP16 <-> FP32
// ref: https://github.com/Maratyszcza/FP16

static inline float fp32_from_bits(uint32_t w) {
    union {
        uint32_t as_bits;
        float as_value;
    } fp32;

    fp32.as_bits = w;
    return fp32.as_value;
}

static inline uint32_t fp32_to_bits(float f) {
    union {
        float as_value;
        uint32_t as_bits;
    } fp32;

    fp32.as_value = f;
    return fp32.as_bits;
}

static inline float az_compute_fp16_to_fp32(az_fp16_t h) {
    const uint32_t w = (uint32_t)h.value << 16;
    const uint32_t sign = w & UINT32_C(0x80000000);
    const uint32_t two_w = w + w;

    const uint32_t exp_offset = UINT32_C(0xE0) << 23;
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) || defined(__GNUC__) && !defined(__STRICT_ANSI__)
    const float exp_scale = 0x1.0p-112f;
#else
    const float exp_scale = fp32_from_bits(UINT32_C(0x7800000));
#endif
    const float normalized_value = fp32_from_bits((two_w >> 4) + exp_offset) * exp_scale;

    const uint32_t magic_mask = UINT32_C(126) << 23;
    const float magic_bias = 0.5f;
    const float denormalized_value = fp32_from_bits((two_w >> 17) | magic_mask) - magic_bias;

    const uint32_t denormalized_cutoff = UINT32_C(1) << 27;
    const uint32_t result =
        sign | (two_w < denormalized_cutoff ? fp32_to_bits(denormalized_value) : fp32_to_bits(normalized_value));
    return fp32_from_bits(result);
}

static inline az_fp16_t az_compute_fp32_to_fp16(float f) {
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) || defined(__GNUC__) && !defined(__STRICT_ANSI__)
    const float scale_to_inf = 0x1.0p+112f;
    const float scale_to_zero = 0x1.0p-110f;
#else
    const float scale_to_inf = fp32_from_bits(UINT32_C(0x77800000));
    const float scale_to_zero = fp32_from_bits(UINT32_C(0x08800000));
#endif
    float base = (fabsf(f) * scale_to_inf) * scale_to_zero;

    const uint32_t w = fp32_to_bits(f);
    const uint32_t shl1_w = w + w;
    const uint32_t sign = w & UINT32_C(0x80000000);
    uint32_t bias = shl1_w & UINT32_C(0xFF000000);
    if (bias < UINT32_C(0x71000000)) {
        bias = UINT32_C(0x71000000);
    }

    base = fp32_from_bits((bias >> 1) + UINT32_C(0x07800000)) + base;
    const uint32_t bits = fp32_to_bits(base);
    const uint32_t exp_bits = (bits >> 13) & UINT32_C(0x00007C00);
    const uint32_t mantissa_bits = bits & UINT32_C(0x00000FFF);
    const uint32_t nonsign = exp_bits + mantissa_bits;
    return (az_fp16_t){.value =
                           (uint16_t)((sign >> 16) | (shl1_w > UINT32_C(0xFF000000) ? UINT16_C(0x7E00) : nonsign))};
}

#define AZ_COMPUTE_FP16_TO_FP32(x) az_compute_fp16_to_fp32(x)
#define AZ_COMPUTE_FP32_TO_FP16(x) az_compute_fp32_to_fp16(x)

#endif  // __F16C__

#endif  // __ARM_NEON

// precomputed f32 table for f16 (256 KB)
extern float az_fp16_to_fp32_table[1 << 16];

void az_precompute_fp16_to_fp32_table(void);

// On ARM NEON, it's quicker to directly convert x -> x instead of calling into az_lookup_fp16_to_fp32,
// so we define AZ_FP16_TO_FP32 and AZ_FP32_TO_FP16 elsewhere for NEON.
// This is also true for POWER9.
#if !defined(AZ_FP16_TO_FP32) || !defined(AZ_FP32_TO_FP16)

inline static float az_lookup_fp16_to_fp32(az_fp16_t f) {
    union {
        uint16_t s;
        az_fp16_t f;
    } cvt;

    cvt.f = f;
    return az_fp16_to_fp32_table[cvt.s];
}

#define AZ_FP16_TO_FP32(x) az_lookup_fp16_to_fp32(x)
#define AZ_FP32_TO_FP16(x) AZ_COMPUTE_FP32_TO_FP16(x)

#endif

#if defined(__cplusplus)
}
#endif
