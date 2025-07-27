#pragma once

#include <cstddef>
#include<functional>
#include "powerinfer-cpu-param.hpp"

struct FusedSparseFFN {
    const HostComputeParam param;
    size_t batch_size = 0;
    size_t embed_dim = 0;
    size_t ffn_hidden_dim = 0;
    const char *up_weight = nullptr;  
    const char *gate_weight = nullptr;  
    const char *down_weight = nullptr;  //  transposed
    const float *activation = nullptr;
    const float *router_out = nullptr;
    float *output = nullptr;
    char *wdata = nullptr;
    void forward();
};
