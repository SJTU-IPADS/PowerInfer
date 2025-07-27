#pragma once

#include <cstddef>
#include <functional>
#include "powerinfer-cpu-param.hpp"

namespace fused_sparse_moe {

struct FusedSparseMoE {
    const HostComputeParam param;
    size_t batch_size = 0;
    size_t embed_dim = 0;
    size_t ffn_hidden_dim = 0;
    size_t n_expert_used = 0;
    const char *up_weight = nullptr;  
    const char *gate_weight = nullptr;  
    const char *down_weight = nullptr;  // transposed
    const float *activation = nullptr;
    const int32_t* selected_experts;
    const float* expert_weights;
    float *output = nullptr;
    char *wdata = nullptr;
    void forward();
};

}
