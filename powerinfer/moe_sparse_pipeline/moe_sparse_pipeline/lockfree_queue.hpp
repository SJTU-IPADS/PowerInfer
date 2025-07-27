#pragma once

#include "powerinfer-disk-queue.hpp"

namespace moe_sparse_pipeline {

template <typename T>
struct LockfreeQueue {
    explicit LockfreeQueue(size_t reserved_size) {
        init_lockfree_queue(&queue, reserved_size);
    }

    ~LockfreeQueue() {
        destroy_lockfree_queue(&queue);
    }

    void enqueue(T *data) {
        lockfree_enqueue(&queue, data);
    }

    auto dequeue(bool wait) -> T * {
        return lockfree_dequeue(&queue, wait);
    }

private:
    lockfree_queue queue;
};

}
