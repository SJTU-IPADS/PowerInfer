#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#include "powerinfer-log.hpp"

namespace moe_sparse_pipeline {

template <typename T, size_t pool_size>
struct ObjectPool {
    static constexpr size_t alignment = 512;

    ObjectPool() {
        size_t total_size = pool_size * sizeof(T);
        if (total_size % alignment != 0) {
            total_size += alignment - (total_size % alignment);
        }
        pool = static_cast<T*>(aligned_alloc(alignment, total_size));
        POWERINFER_ASSERT(pool);

        next_available = 0;
    }

    ~ObjectPool() {
        release_all();
        free(pool);
    }

    // Allocate one object from the pool
    template <typename ... Args>
    auto allocate(Args && ... args) -> T * {
        POWERINFER_ASSERT(next_available < pool_size);
        T* ptr = &pool[next_available];
        new (ptr) T(std::forward<Args>(args)...);
        ++next_available;
        return ptr;
    }

    // Release all objects in the pool
    void release_all() {
        for (size_t i = 0; i < next_available; ++i) {
            pool[i].~T();
        }
        next_available = 0;
    }

private:
    T* pool = nullptr;          // Memory block aligned to `alignment` bytes
    size_t next_available = 0;  // Next available object index
};

}  // namespace moe_sparse_pipeline
