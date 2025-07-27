#pragma once

#include "az/core/spin_barrier.hpp"
#include "az/core/utils.hpp"

namespace az {

/**
 * Each worker will receive its own WorkerInfo, with different id.
 */
struct WorkerInfo {
    size_t group_size;
    size_t id;
    SpinBarrier *barrier = nullptr;

    WorkerInfo() = default;

    WorkerInfo(size_t group_size, size_t id, SpinBarrier &barrier) :
        group_size(group_size),
        id(id),
        barrier(&barrier) {}

    void arrive_and_wait() const {
        barrier->wait();
    }

    /**
     * Distribute `n` items among this worker group, and return the corresponding chunk of the current worker.
     */
    auto distribute(size_t n, size_t block_size = 1) const {
        return distribute_items(n, group_size, id, block_size);
    }
};

}  // namespace az
