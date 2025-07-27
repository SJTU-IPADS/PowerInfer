#include <cassert>
#include <cstdio>
#include <atomic>

#include "powerinfer-disk-queue.hpp"
#include "atomic-queue/queue.h"

// TODO(mixtral): Hardcoded queue size
constexpr size_t concurrent_queue_size = 1024;

using ConcurrentQueue = atomic_queue::AtomicQueue<void *, concurrent_queue_size>;

void init_lockfree_queue(struct lockfree_queue *queue, size_t reserved_size) {
    if (reserved_size > concurrent_queue_size) {
        fprintf(
            stderr, "reserved_size=%zu must be no larger than %zu\n", reserved_size, concurrent_queue_size
        );
        abort();
    }

    queue->instance = new ConcurrentQueue();
}

void destroy_lockfree_queue(struct lockfree_queue *queue) {
    delete (ConcurrentQueue *)queue->instance;
}

void lockfree_enqueue(struct lockfree_queue *queue, void *data) {
    auto *instance = (ConcurrentQueue *)queue->instance;
    instance->push(data);
}

void lockfree_enqueue_bulk(struct lockfree_queue *queue, void *data[], size_t count) {
    auto *instance = (ConcurrentQueue *)queue->instance;
    for (size_t i = 0; i < count; i++) {
        instance->push(data[i]);
    }
}

void *lockfree_dequeue(struct lockfree_queue *queue, bool wait) {
    auto *instance = (ConcurrentQueue *)queue->instance;

    if (wait) {
        return instance->pop();
    } else {
        void *data;
        if (instance->try_pop(data)) {
            return data;
        } else {
            return NULL;
        }
    }
}
