#pragma once

#include <cstddef>
#include <cstdint>

// Validate the row indices stored in a GPU split sidecar before they are used
// for host pointer arithmetic.
inline bool gpu_bucket_indices_are_valid(const int32_t * indices, size_t count, int64_t source_rows) {
    if (indices == nullptr || count == 0 || source_rows <= 0 || count > static_cast<size_t>(source_rows)) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (indices[i] < 0 || static_cast<int64_t>(indices[i]) >= source_rows) {
            return false;
        }
    }

    return true;
}
