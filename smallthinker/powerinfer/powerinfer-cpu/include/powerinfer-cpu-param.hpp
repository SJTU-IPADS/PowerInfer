//
// Created by Test on 12/9/2024.
//

#pragma once

#include <barrier>

#include "powerinfer-perf.hpp"

struct HostComputeParam {
    int             ith;
    int             nth;
    void *          wdata;
    size_t          wsize; 
    std::barrier<> *barrier;
    int             barrier_count;

    void arrive_and_wait() const {
        PerfEvent _(__PRETTY_FUNCTION__);
        barrier->wait(barrier->arrive(barrier_count));
    }
};

inline int ggml_param_get_ith(const HostComputeParam *param) { return param->ith; }

inline int ggml_param_get_nth(const HostComputeParam *param) { return param->nth; }

inline void ggml_param_barrier(const HostComputeParam *param) { param->barrier->arrive_and_wait(); }

#ifndef POWERINFER_CPU_PARAM_DEF
#define POWERINFER_CPU_PARAM_DEF

    struct PowerInferCPUParam {
        int     loader_id; 
        int     ith;
        int     nth; 
        void *  wdata;
        size_t  wsize; 
    };
    
#endif // POWERINFER_CPU_PARAM_DEF
