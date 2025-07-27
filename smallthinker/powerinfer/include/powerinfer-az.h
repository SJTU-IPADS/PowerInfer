#pragma once

// TODO: HACK: TO BE REMOVE

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "powerinfer-api.h"

#if defined(__cplusplus)
extern "C" {
#endif

    POWERINFER_API void az_linear_q4_forward(int32_t handle, size_t batch_size, float *out, const float *in, size_t n_workers, size_t worker_id);

    POWERINFER_API int64_t az_get_linear_q4_max_batch_size(int32_t handle);
    POWERINFER_API int64_t az_get_linear_q4_in_features(int32_t handle);
    POWERINFER_API int64_t az_get_linear_q4_out_features(int32_t handle);

#if defined(__cplusplus)
}
#endif