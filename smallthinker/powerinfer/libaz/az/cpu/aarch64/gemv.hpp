#include "az/common.hpp"
#include "az/core/fp16.h"
#include "az/cpu/quant_types.hpp"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace az::cpu {

#define QK4_0 32
#define QK8_0 32
#define QK4_0_S128 128
#define QK8_0_S128 128

template <int K>
constexpr int QK_0() {
    if constexpr (K == 4) {
        return QK4_0;
    }
    if constexpr (K == 8) {
        return QK8_0;
    }
    return -1;
}

template <int K, int N>
struct block {
    az_fp16_t d[N];                      // deltas for N qK_0 blocks
    int8_t qs[(QK_0<K>() * N * K) / 8];  // quants for N qK_0 blocks
};

using block_q4_0x4 = block<4, 4>;
using block_q4_0x8 = block<4, 8>;
using block_q8_0x4 = block<8, 4>;
using block_q8_0x8 = block<8, 8>;


#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif

template <typename BLOC_TYPE, int64_t INTER_SIZE, int64_t NB_COLS>
void gemv(int n, float *s, size_t bs, const void *vx, const void *vy, int nr, int nc);
}  // namespace az::cpu
