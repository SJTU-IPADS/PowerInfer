#pragma once

#include <cstddef>

#include "powerinfer-cpu-data.hpp"
#include "vdot.hpp"

void chunked_vec_dot_fp32_act_q8_0_weight_q4_0_row_4(
    size_t n,
    const char * __restrict__ act,
    const char * __restrict__ weight,
    float * __restrict__ out
) {
    constexpr size_t qk = 32;
    const size_t n_blocks = n / qk;

    const block_q8_0 *act_blocks = reinterpret_cast<const block_q8_0 *>(act);

#if defined(__ARM_NEON)
    float32x4_t vsum0 = vdupq_n_f32(0);
    float32x4_t vsum1 = vdupq_n_f32(0);
    float32x4_t vsum2 = vdupq_n_f32(0);
    float32x4_t vsum3 = vdupq_n_f32(0);

    const uint8x16_t m4b = vdupq_n_u8(0x0F);
    const int8x16_t  s8b = vdupq_n_s8(0x8);

    for (size_t i = 0; i < n_blocks; i++) {
        float act_scale = POWERINFER_FP16_TO_FP32(act_blocks[i].d);

        int8x16_t vx0 = vld1q_s8(act_blocks[i].qs);
        int8x16_t vx1 = vld1q_s8(act_blocks[i].qs + 16);

        {
            const block_q4_0 *weight_blocks = reinterpret_cast<const block_q4_0 *>(weight) + 0 * n_blocks;
            int8x16_t vy_qs = vreinterpretq_s8_u8(vld1q_u8(weight_blocks[i].qs));
            int8x16_t vy0 = vsubq_s8(vreinterpretq_s8_u8(vandq_u8(vy_qs, m4b)), s8b);
            int8x16_t vy1 = vsubq_s8(vreinterpretq_s8_u8(vshrq_n_u8(vy_qs, 4)), s8b);
            int32x4_t p = vdotq_s32_emu(vdotq_s32_emu(vdupq_n_s32(0), vx0, vy0), vx1, vy1);
            vsum0 = vmlaq_n_f32(vsum0, vcvtq_f32_s32(p), act_scale * POWERINFER_FP16_TO_FP32(weight_blocks[i].d));
        }

        {
            const block_q4_0 *weight_blocks = reinterpret_cast<const block_q4_0 *>(weight) + 1 * n_blocks;
            int8x16_t vy_qs = vreinterpretq_s8_u8(vld1q_u8(weight_blocks[i].qs));
            int8x16_t vy0 = vsubq_s8(vreinterpretq_s8_u8(vandq_u8(vy_qs, m4b)), s8b);
            int8x16_t vy1 = vsubq_s8(vreinterpretq_s8_u8(vshrq_n_u8(vy_qs, 4)), s8b);
            int32x4_t p = vdotq_s32_emu(vdotq_s32_emu(vdupq_n_s32(0), vx0, vy0), vx1, vy1);
            vsum1 = vmlaq_n_f32(vsum1, vcvtq_f32_s32(p), act_scale * POWERINFER_FP16_TO_FP32(weight_blocks[i].d));
        }

        {
            const block_q4_0 *weight_blocks = reinterpret_cast<const block_q4_0 *>(weight) + 2 * n_blocks;
            int8x16_t vy_qs = vreinterpretq_s8_u8(vld1q_u8(weight_blocks[i].qs));
            int8x16_t vy0 = vsubq_s8(vreinterpretq_s8_u8(vandq_u8(vy_qs, m4b)), s8b);
            int8x16_t vy1 = vsubq_s8(vreinterpretq_s8_u8(vshrq_n_u8(vy_qs, 4)), s8b);
            int32x4_t p = vdotq_s32_emu(vdotq_s32_emu(vdupq_n_s32(0), vx0, vy0), vx1, vy1);
            vsum2 = vmlaq_n_f32(vsum2, vcvtq_f32_s32(p), act_scale * POWERINFER_FP16_TO_FP32(weight_blocks[i].d));
        }

        {
            const block_q4_0 *weight_blocks = reinterpret_cast<const block_q4_0 *>(weight) + 3 * n_blocks;
            int8x16_t vy_qs = vreinterpretq_s8_u8(vld1q_u8(weight_blocks[i].qs));
            int8x16_t vy0 = vsubq_s8(vreinterpretq_s8_u8(vandq_u8(vy_qs, m4b)), s8b);
            int8x16_t vy1 = vsubq_s8(vreinterpretq_s8_u8(vshrq_n_u8(vy_qs, 4)), s8b);
            int32x4_t p = vdotq_s32_emu(vdotq_s32_emu(vdupq_n_s32(0), vx0, vy0), vx1, vy1);
            vsum3 = vmlaq_n_f32(vsum3, vcvtq_f32_s32(p), act_scale * POWERINFER_FP16_TO_FP32(weight_blocks[i].d));
        }
    }

    out[0] = vaddvq_f32(vsum0);
    out[1] = vaddvq_f32(vsum1);
    out[2] = vaddvq_f32(vsum2);
    out[3] = vaddvq_f32(vsum3);
#else
    POWERINFER_ASSERT(false);
#endif
}
