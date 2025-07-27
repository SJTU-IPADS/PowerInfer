#pragma once

#include <cstddef>

class PowerInferPool {
public:
    PowerInferPool() = default;

    virtual ~PowerInferPool() noexcept = default;

public:
    virtual void *alloc(size_t size, size_t *actual_size, const char *reason, void *stream) = 0;

    virtual void free(void *ptr, size_t size, const char *reason, void *stream) = 0;

    virtual void clear() = 0;
};
