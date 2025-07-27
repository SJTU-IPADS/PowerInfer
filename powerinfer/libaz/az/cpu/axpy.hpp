#pragma once

#include "az/common.hpp"
#include "az/core/fp16.h"

namespace az::cpu {

void axpy_fp32(size_t n, float *out, float scale, const float *in);
void axpy_fp32_in_fp16(size_t n, float *out, float scale, const az_fp16_t *in);

}  // namespace az::cpu
