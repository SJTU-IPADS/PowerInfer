#pragma once

#include "az/core/fp16.h"

#include <vector>

namespace az::cpu {

extern std::vector<float> silu_fp16_to_fp32_lut;

float silu_fp32(float x);
float lut_silu_fp16_to_fp32(az_fp16_t x);

void init_silu_lut();

}  // namespace az::cpu
