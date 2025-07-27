#pragma once

#include "az/common.hpp"
#include "az/core/fp16.h"

namespace az::cpu {

struct block_q4_0 {
    static constexpr size_t block_size = 32;

    az_fp16_t d;
    uint8_t qs[block_size / 2];

    static constexpr size_t row_size(size_t n) {
        return (n / block_size) * sizeof(block_q4_0);
    }
};


struct block_q8_0 {
    static constexpr size_t block_size = 32;

    az_fp16_t d;
    int8_t qs[block_size];

    static constexpr size_t row_size(size_t n) {
        return (n / block_size) * sizeof(block_q8_0);
    }
};


void quantize_row_q4_0(block_q4_0 *out, const float *in, size_t n);
void dequantize_row_q4_0(float *out, const block_q4_0 *in, size_t n);
void quantize_row_q8_0(block_q8_0 *out, const float *in, size_t n);

}  // namespace az::cpu
