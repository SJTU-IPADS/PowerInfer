#pragma once


#include <cstdint>
#include <cstring>

#include "powerinfer-macro.hpp"
#include "powerinfer-type.hpp"

#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif // __ARM_FEATURE_SVE

#if defined(__ARM_NEON)
    #include <arm_neon.h>
#endif // __ARM_NEON

struct block_q4_0 {
    uint16_t d;           // delta
    uint8_t  qs[QK4_0 / 2]; // nibbles / quants
};


struct block_q8_0 {
    uint16_t d;           // delta
    int8_t   qs[QK8_0]; // nibbles / quants
};

extern "C" HOST_API float ggml_lookup_fp16_to_fp32(ggml_fp16_t f);

#if defined(__ARM_NEON)
    typedef __fp16 ggml_fp16_internal_t;
#endif

#if defined(__ARM_NEON) && !defined(_MSC_VER)
    #define POWERINFER_COMPUTE_FP16_TO_FP32(x) ggml_compute_fp16_to_fp32(x)
    #define POWERINFER_COMPUTE_FP32_TO_FP16(x) ggml_compute_fp32_to_fp16(x)

    #define POWERINFER_FP16_TO_FP32(x) ggml_compute_fp16_to_fp32(x)

    static inline float ggml_compute_fp16_to_fp32(ggml_fp16_t h) {
        ggml_fp16_internal_t tmp;
        std::memcpy(&tmp, &h, sizeof(ggml_fp16_t));
        return (float)tmp;
    }

    static inline ggml_fp16_t ggml_compute_fp32_to_fp16(float f) {
        ggml_fp16_t res;
        ggml_fp16_internal_t tmp = f;
        std::memcpy(&res, &tmp, sizeof(ggml_fp16_t));
        return res;
    }
#else
    #include <immintrin.h>

    #ifdef _MSC_VER
        inline float       ggml_compute_fp16_to_fp32(const ggml_fp16_t x) { return _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128(x))); }
        inline ggml_fp16_t ggml_compute_fp32_to_fp16(const float x)       { return _mm_extract_epi16(_mm_cvtps_ph(_mm_set_ss(x), 0), 0);   }
    #else // NOT _MSC_VER
        inline float       ggml_compute_fp16_to_fp32(const ggml_fp16_t x) { return _cvtsh_ss(x); }
        inline ggml_fp16_t ggml_compute_fp32_to_fp16(const float x)       { return _cvtss_sh(x, 0); }
    #endif // _MSC_VER
#endif


#ifndef POWERINFER_FP16_TO_FP32
    #define POWERINFER_FP16_TO_FP32(x) ggml_lookup_fp16_to_fp32(x)
#endif // POWERINFER_FP16_TO_FP32

#ifndef POWERINFER_FP32_TO_FP16
    #define POWERINFER_FP32_TO_FP16(x) ggml_compute_fp32_to_fp16(x)
#endif
