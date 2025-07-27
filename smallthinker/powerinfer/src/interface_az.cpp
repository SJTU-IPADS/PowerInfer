#include "powerinfer-az.h"

#include "powerinfer-az.h"

#include "az/core/handle.hpp"
#include "az/core/spin_barrier.hpp"


extern "C" {

void az_linear_q4_forward(int32_t handle, size_t batch_size, float *out, const float *in, size_t n_workers, size_t worker_id) {
    AZ_UNUSED(handle);
    AZ_UNUSED(batch_size);
    AZ_UNUSED(out);
    AZ_UNUSED(in);
    AZ_UNUSED(n_workers);
    AZ_UNUSED(worker_id);
    abort();
}

int64_t az_get_linear_q4_max_batch_size(int32_t handle) {
    AZ_UNUSED(handle);
    abort();
}

int64_t az_get_linear_q4_in_features(int32_t handle) {
    AZ_UNUSED(handle);
    abort();
}

int64_t az_get_linear_q4_out_features(int32_t handle) {
    AZ_UNUSED(handle);
    abort();
}

}
