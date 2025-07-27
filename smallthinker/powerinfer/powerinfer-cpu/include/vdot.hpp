#pragma once

#if defined(__ARM_NEON)

#include "arm_neon.h"

#if !defined(__ARM_FEATURE_DOTPROD)

inline static int32x4_t vdotq_s32_emu(int32x4_t acc, int8x16_t a, int8x16_t b) {
    const int16x8_t p0 = vmull_s8(vget_low_s8 (a), vget_low_s8 (b));
    const int16x8_t p1 = vmull_s8(vget_high_s8(a), vget_high_s8(b));
    return vaddq_s32(acc, vaddq_s32(vpaddlq_s16(p0), vpaddlq_s16(p1)));
}

#else

#define vdotq_s32_emu(a, b, c) vdotq_s32(a, b, c)

#endif  // !defined(__ARM_FEATURE_DOTPROD)
#endif  // defined(__ARM_NEON)
