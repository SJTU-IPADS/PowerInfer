#pragma once

#include "az/cpu/quant_types.hpp"

namespace az::cpu {

float vec_dot_q4_0_q8_0(size_t n, const void *__restrict__ vx, const void * __restrict__ vy);
float vec_dot_q4_0_s128_q8_0_s128(size_t n, const void *vx, const void *vy);

}  // namespace az::cpu
