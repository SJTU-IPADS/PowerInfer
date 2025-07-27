#pragma once

#include <cstddef>
#include <string>
#include <cstdio>

namespace moe_sparse_pipeline {

static constexpr size_t max_batch_size = 8;
static constexpr size_t max_n_experts = 128;
static constexpr size_t io_queue_depth = 512;
static constexpr size_t io_alignment = 4096;


template <typename T>
T getenv_(const std::string &name, const T &default_value) {
    auto env = getenv(name.c_str());
    if (env) {
        if constexpr (std::is_integral_v<T>) {
            if(atoll(env)<io_queue_depth)
            {
                fprintf(stderr,"Error: MAX_N_CACHED must > %zu",io_queue_depth);
                exit(-1);
            }
            return atoll(env);
        } else if constexpr (std::is_floating_point_v<T>) {
            return atof(env);
        } else {
            return std::string(env);
        }
    } else {
        return default_value;
    }
}



/*
    max_n_cached_matrices determines the maximum number of matrices that can be stored in system memory at the same time. 
    You can adjust the cache size according to memory limitations.

    For SmallThinker 21B:  52 moe layer * 64 experts * 3 matrices (up, gate, down) per expert (PS: One Q4_0 matrices sized 768 * 2560, is about 1.1MB)
    For 8GB mem: 3 * 64 * 32 is recommended

    For SmallThinker 4B:  32 moe layer * 32 experts * 3 matrices (up, gate, down) per expert (PS: One Q4_0 matrices sized 768 * 3584, is about 1.5MB)
    For 1GB mem: 3 * 32 * 8 is recommended
*/
static size_t max_n_cached_matrices=getenv_("MAX_N_CACHED",3 * 64 * 32);


static bool iou_enable_sq_poll = false;
static int iou_sq_poll_cpu_affinity = -1;
static int io_worker_cpu_affinity = -1;

}
