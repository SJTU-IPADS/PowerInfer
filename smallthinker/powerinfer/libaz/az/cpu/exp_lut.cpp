#include "az/cpu/exp_lut.hpp"

#include <cmath>
#include <cstring>

namespace az::cpu {

std::vector<az_fp16_t> exp_fp16_lut;
std::vector<float> exp_fp16_to_fp32_lut;

az_fp16_t lut_exp_fp16(az_fp16_t x) {
    uint16_t i;
    memcpy(&i, &x, sizeof(x));
    return exp_fp16_lut[i];
}

float lut_exp_fp16_to_fp32(az_fp16_t x) {
    uint16_t i;
    memcpy(&i, &x, sizeof(x));
    return exp_fp16_to_fp32_lut[i];
}

void init_exp_lut() {
    exp_fp16_to_fp32_lut.resize(UINT16_MAX);
    exp_fp16_lut.resize(UINT16_MAX);

    for (size_t i = 0; i < UINT16_MAX; i++) {
        az_fp16_t v;
        memcpy(&v, &i, sizeof(v));
        exp_fp16_to_fp32_lut[i] = std::exp(AZ_FP16_TO_FP32(v));
        exp_fp16_lut[i] = AZ_FP32_TO_FP16(exp_fp16_to_fp32_lut[i]);
    }
}

}  // namespace az::cpu
