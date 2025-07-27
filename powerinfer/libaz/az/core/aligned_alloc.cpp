#include "az/core/aligned_alloc.hpp"

#include <cstdlib>

namespace az {

void *aligned_alloc(size_t alignment, size_t size) {
    // Align up size to surpass some warnings from sanitizers
    size_t remainder = size % alignment;
    if (remainder != 0) {
        size += alignment - remainder;
    }

#if defined(_MSC_VER)
    // Microsoft guys are unique and write in reversed order
    return ::_aligned_malloc(size, alignment);
#else
    return ::aligned_alloc(alignment, size);
#endif
}

void aligned_free(void *ptr) {
#if defined(_MSC_VER)
    ::_aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

}  // namespace az
