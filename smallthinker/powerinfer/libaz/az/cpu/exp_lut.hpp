#pragma once

#include "az/core/fp16.h"

#include <vector>

namespace az::cpu {

extern std::vector<az_fp16_t> exp_fp16_lut;
extern std::vector<float> exp_fp16_to_fp32_lut;

az_fp16_t lut_exp_fp16(az_fp16_t x);
float lut_exp_fp16_to_fp32(az_fp16_t x);

void init_exp_lut();

}  // namespace az::cpu
