#pragma once

#include <cstddef>

#include "powerinfer-type.hpp"

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC) ||  defined(__F16C__)
#define NO_AXPY_USE_FP16
#endif

#if defined(AXPY_USE_FP16)
using axpy_out_type = ggml_fp16_t;
constexpr size_t axpy_batch_size = 8;
#else
using axpy_out_type = float;
constexpr size_t axpy_batch_size = 8;
#endif

void az_axpy_fp16_act_fp32_batch_8_weight_q4_0(
    size_t n, size_t batch_size, const float a[8], const void * __restrict__ vx[8], ggml_fp16_t * __restrict__ y
);

void az_axpy_fp32_act_fp32_batch_8_weight_q4_0(
    size_t n, size_t batch_size, const float a[8], const void * __restrict__ vx[8], float * __restrict__ y
);

void axpy_reduce_f32(size_t vec_dim, size_t n_inputs, const axpy_out_type *inputs[], float *output,bool do_accumulate,float scale);

struct AxpyBatch {
    const size_t vec_dim = 0;
    size_t batch_size = 0;
    const void *vx[axpy_batch_size] = {};
    float a[axpy_batch_size] = {};

#if defined(AXPY_USE_FP16)
    ggml_fp16_t *const vy = nullptr;

    explicit AxpyBatch(size_t vec_dim, ggml_fp16_t *vy) : vec_dim(vec_dim), vy(vy) {}
#else
    float *const vy = nullptr;

    explicit AxpyBatch(size_t vec_dim, float *vy) : vec_dim(vec_dim), vy(vy) {}
#endif

    void enqueue(float a, const void *vx);
    void flush();
};
