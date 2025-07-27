#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "powerinfer-log.hpp"

constexpr float SPARSE_PRED_THRESHOLD = 0;

constexpr int SPARSE_MOE_MAX_NUM_HEAD   = 16;
constexpr int SPARSE_MOE_NUM_HEAD       = 6;
constexpr int SPARSE_MOE_HEAD_SIZE      = 896;

#ifndef POWERINFER_TYPE_DEF
#define POWERINFER_TYPE_DEF

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

        enum powerinfer_type {
            POWERINFER_TYPE_Q4_0 = 0,
            POWERINFER_TYPE_Q8_0 = 1,
            POWERINFER_TYPE_F16  = 2,
            POWERINFER_TYPE_F32  = 3,
            POWERINFER_TYPE_Q6_K = 4,
            POWERINFER_TYPE_Q4_0_S128 =5,
            POWERINFER_TYPE_Q8_0_S128 =6
        };

        struct PowerInferKVCacheShape {
            powerinfer_type   type;
            uint32_t    num_context;
            uint32_t    num_embd_head_k;
            uint32_t    num_embd_head_v;
            uint32_t    num_head_kv;
            uint32_t    num_head_q;
        };
    
        struct PositionEmbeddingMetadata {
            int64_t max_position_embeddings;
            int64_t origin_max_position_embeddings;
            int64_t rotary_dim;
        
            float   freq_base;
            float   freq_scale;
            float   ext_factor;
            float   attn_factor;
            float   beta_fast;
            float   beta_slow;
        };

#ifdef __cplusplus
    }
#endif // __cplusplus

#endif // POWERINFER_TYPE_DEF

constexpr int QK4_0 = GROUP_SIZE;
constexpr int QR4_0 = 2;
constexpr int QK4_0_S128 = 128;
constexpr int QI4_0 = (QK4_0 / (4 * QR4_0));

constexpr int QK8_0 = 32;
constexpr int QK8_0_S128 = 128;
constexpr int QR8_0 = 1;
constexpr int QI8_0 = (QK8_0 / (4 * QR8_0));

constexpr int QK8_1 = 32;

struct block_q4_0_ref {
    uint16_t d;           // delta
    uint8_t qs[QK4_0 / 2]; // nibbles / quants
};


struct block_q8_0_ref {
    uint16_t d;           // delta
    int8_t   qs[QK8_0]; // nibbles / quants
};


constexpr int QK_K = 256;
constexpr int QR6_K = 2;
constexpr int QI6_K = (QK_K / (4*QR6_K));

struct block_q6_k_ref {
    uint8_t ql[QK_K/2];      // quants, lower 4 bits
    uint8_t qh[QK_K/4];      // quants, upper 2 bits
    int8_t  scales[QK_K/16]; // scales, quantized with 8 bits
    uint16_t d;             // super-block scale
};

constexpr int QR8_1 = 1;
constexpr int QI8_1 = (QK8_1 / (4 * QR8_1));

struct block_q8_1_ref {
    union {
        struct {
            uint16_t d; // delta
            uint16_t s; // d * sum(qs[i])
        } data;
        float ds;
    };
    int8_t qs[QK8_1]; // quants
};

typedef uint16_t ggml_fp16_t;

template<typename T>
    requires std::is_integral_v<T>
inline constexpr T powerinfer_row_size(enum powerinfer_type type, T num_element) {
    switch(type) {
        case POWERINFER_TYPE_Q4_0:
            return num_element / QK4_0 * sizeof(block_q4_0_ref);
        case POWERINFER_TYPE_Q8_0:
            return num_element / QK8_0 * sizeof(block_q8_0_ref);
        case POWERINFER_TYPE_F16:
            return 2 * num_element;
        case POWERINFER_TYPE_F32:
            return 4 * num_element;
        case POWERINFER_TYPE_Q6_K:
            return num_element / QK_K * sizeof(block_q6_k_ref);
        default:
            POWERINFER_ABORT("unsupported type");
            POWERINFER_UNREACHABLE();
    }
    return 0;
}

/*!
 * @return The size of K cache of one token
 */
inline size_t size_k_byte(const PowerInferKVCacheShape &shape) {
    return powerinfer_row_size(shape.type, shape.num_embd_head_k) * shape.num_context * shape.num_head_kv;
}

/*!
 * @return The size of V cache of one token
 */
inline size_t size_v_byte(const PowerInferKVCacheShape &shape) {
    return powerinfer_row_size(shape.type, shape.num_context) * shape.num_embd_head_v * shape.num_head_kv;
}
