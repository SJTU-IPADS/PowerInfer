#pragma once

#ifndef POWERINFER_TYPE_DEF
#define POWERINFER_TYPE_DEF

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

        enum powerinfer_type {
            POWERINFER_TYPE_Q4_0 = 0,
            POWERINFER_TYPE_Q8_0 = 1,
            POWERINFER_TYPE_F16  = 2,
            POWERINFER_TYPE_F32  = 3,
            POWERINFER_TYPE_Q6_K = 4
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
