#include "az/cpu/axpy.hpp"

#include "az/assert.hpp"
#include "az/core/intrinsics.hpp"

namespace az::cpu {

void axpy_fp32(size_t n, float *out, float scale, const float *in) {
    for (size_t i = 0; i < n; i++) {
        out[i] += scale * in[i];
    }
}

void axpy_fp32_in_fp16(size_t n, float *out, float scale, const az_fp16_t *in) {
#if defined(__ARM_NEON)
    AZ_ASSERT(n % 4 == 0);
    for (size_t i = 0; i < n; i += 4) {
        float16x4_t vx = vld1_f16((__fp16 *)in + i);
        float32x4_t vy = vld1q_f32(out + i);
        vy = vfmaq_n_f32(vy, vcvt_f32_f16(vx), scale);
        vst1q_f32(out + i, vy);
    }
#else
    for (size_t i = 0; i < n; i++) {
        out[i] += scale * AZ_FP16_TO_FP32(in[i]);
    }
#endif
}

}  // namespace az::cpu
