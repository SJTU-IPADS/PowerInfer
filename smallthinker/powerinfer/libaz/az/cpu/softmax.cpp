#include "az/cpu/softmax.hpp"

#include "az/cpu/exp_lut.hpp"

#include <cmath>

namespace az::cpu {

float softmax_fp32(size_t n, float *out, const float *in, float vmax) {
    size_t i = 0;
    float sum = 0;
#if defined(__ARM_NEON)
    for (; i + 3 < n; i += 4) {
        float32x4_t val = vexp_fp32(vsubq_f32(vld1q_f32(in + i), vdupq_n_f32(vmax)));
        vst1q_f32(out + i, val);
        sum += (float)vaddvq_f32(val);
    }
#endif
    for (; i < n; ++i) {
        float val = expf(in[i] - vmax);
        sum += (float)val;
        out[i] = val;
    }
    return sum;
}

void softmax_scale_fp16(size_t n, az_fp16_t *out, const az_fp16_t *in) {
    float vmax = -INFINITY;
    for (size_t i = 0; i < n; i++) {
        float v = AZ_FP16_TO_FP32(in[i]);
        vmax = std::max(vmax, v);
    }

    float exp_sum = 0;
    for (size_t i = 0; i < n; i++) {
        float v = AZ_FP16_TO_FP32(in[i]);
        out[i] = lut_exp_fp16(AZ_FP32_TO_FP16(v - vmax));
        exp_sum += AZ_FP16_TO_FP32(out[i]);
    }

    float scale = 1.0f / exp_sum;
    for (size_t i = 0; i < n; i++) {
        out[i] = AZ_FP32_TO_FP16(AZ_FP16_TO_FP32(out[i]) * scale);
    }
}

void softmax_scale_fp32(size_t n, float *out, const float *in, float *out_vmax) {
    float vmax = -INFINITY;
    size_t max_index = 0;
    for (size_t i = 0; i < n; i++) {
        if (in[i] > vmax) {
            vmax = in[i];
            max_index = i;
        }
    }

    float exp_sum = 0;
    for (size_t i = 0; i < n; i++) {
        out[i] = lut_exp_fp16_to_fp32(AZ_FP32_TO_FP16(out[i] - vmax));
        exp_sum += out[i];
    }

    float scale = 1.0f / exp_sum;
    for (size_t i = 0; i < n; i++) {
        out[i] = out[i] * scale;
    }

    if (out_vmax) {
        *out_vmax = out[max_index];
    }
}

void softmax_scale_fp16_to_fp32(size_t n, float *out, const az_fp16_t *in, float *out_vmax) {
    float vmax = -INFINITY;
    size_t max_index = 0;
    for (size_t i = 0; i < n; i++) {
        out[i] = AZ_FP16_TO_FP32(in[i]);
        if (out[i] > vmax) {
            vmax = out[i];
            max_index = i;
        }
    }

    float exp_sum = 0;
    for (size_t i = 0; i < n; i++) {
        out[i] = lut_exp_fp16_to_fp32(AZ_FP32_TO_FP16(out[i] - vmax));
        exp_sum += out[i];
    }

    float scale = 1.0f / exp_sum;
    for (size_t i = 0; i < n; i++) {
        out[i] = out[i] * scale;
    }

    if (out_vmax) {
        *out_vmax = out[max_index];
    }
}

}  // namespace az::cpu
