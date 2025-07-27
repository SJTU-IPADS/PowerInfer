#include "az/cpu/silu_lut.hpp"

#include <cmath>
#include <cstring>

namespace az::cpu {

std::vector<float> silu_fp16_to_fp32_lut;

float silu_fp32(float x) {
    return x / (1.0f + std::exp(-x));
}

float lut_silu_fp16_to_fp32(az_fp16_t x) {
    uint16_t i;
    memcpy(&i, &x, sizeof(x));
    return silu_fp16_to_fp32_lut[i];
}

void init_silu_lut() {
    silu_fp16_to_fp32_lut.resize(UINT16_MAX);

    for (size_t i = 0; i < UINT16_MAX; i++) {
        az_fp16_t v;
        memcpy(&v, &i, sizeof(v));
        silu_fp16_to_fp32_lut[i] = silu_fp32(AZ_FP16_TO_FP32(v));
    }
}

}  // namespace az::cpu
