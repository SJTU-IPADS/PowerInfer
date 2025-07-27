#pragma once

#include "powerinfer-cpu-data.hpp"

namespace moe_sparse_pipeline {

static constexpr size_t quant_block_size = 32;
static constexpr size_t vec_dot_block_size = 4;

// TODO: move to include file?
template <int K> constexpr int QK_0() {
    if constexpr (K == 4) {
        return QK4_0;
    }
    if constexpr (K == 8) {
        return QK8_0;
    }
    return -1;
}

template <int K, int N> struct block {
    ggml_fp16_t d[N];                         // deltas for N qK_0 blocks
    int8_t    qs[(QK_0<K>() * N * K) / 8];  // quants for N qK_0 blocks
};

// control size
static_assert(sizeof(block<4, 4>) == 4 * sizeof(ggml_fp16_t) + QK8_0 * 2, "wrong block<4,4> size/padding");
static_assert(sizeof(block<4, 8>) == 8 * sizeof(ggml_fp16_t) + QK8_0 * 4, "wrong block<4,8> size/padding");
static_assert(sizeof(block<8, 4>) == 4 * sizeof(ggml_fp16_t) + QK8_0 * 4, "wrong block<8,4> size/padding");
static_assert(sizeof(block<8, 8>) == 8 * sizeof(ggml_fp16_t) + QK8_0 * 8, "wrong block<8,8> size/padding");

using block_q4_0x4 = block<4, 4>;
using block_q4_0x8 = block<4, 8>;
using block_q8_0x4 = block<8, 4>;
using block_q8_0x8 = block<8, 8>;

void gemv_q4_0_4x4_q8_0(int n, float * __restrict__ s, const void * __restrict__ vx, const void * __restrict__ vy, int nc);
auto make_block_q4_0x4(block_q4_0 * in, unsigned int blck_size_interleave) -> block_q4_0x4;
int repack_q4_0_to_q4_0_4_bl(void * __restrict__ out_data, size_t ne0, size_t ne1, int interleave_block, const void * __restrict__ data, size_t data_size);

}
