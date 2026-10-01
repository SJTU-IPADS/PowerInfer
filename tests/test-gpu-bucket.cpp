#include "gpu_bucket.h"

#include <cstdint>

int main() {
    const int32_t valid[] = {0, 2, 4};
    const int32_t negative[] = {0, -1, 2};
    const int32_t upper_bound[] = {0, 5};

    if (!gpu_bucket_indices_are_valid(valid, 3, 5) || gpu_bucket_indices_are_valid(nullptr, 1, 5) ||
        gpu_bucket_indices_are_valid(valid, 0, 5) || gpu_bucket_indices_are_valid(valid, 3, 2) ||
        gpu_bucket_indices_are_valid(negative, 3, 5) || gpu_bucket_indices_are_valid(upper_bound, 2, 5) ||
        gpu_bucket_indices_are_valid(valid, 3, 0)) {
        return 1;
    }

    return 0;
}
