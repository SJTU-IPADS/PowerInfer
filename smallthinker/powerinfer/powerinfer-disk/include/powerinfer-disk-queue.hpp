#pragma once

#include <stddef.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief lock-free queue (C implementation prepared for ggml)
 */
struct lockfree_queue {
    void *instance = nullptr;
};

void init_lockfree_queue(struct lockfree_queue *queue, size_t reserved_size);
void destroy_lockfree_queue(struct lockfree_queue *queue);
void lockfree_enqueue(struct lockfree_queue *queue, void *data);
void lockfree_enqueue_bulk(struct lockfree_queue *queue, void *data[], size_t count);
void *lockfree_dequeue(struct lockfree_queue *queue, bool wait);

#if defined(__cplusplus)
}
#endif

#if defined (__cplusplus)
#include <condition_variable>
#include <mutex>
#include <vector>
#include <optional>

/**
 * @brief thread-safe queue (used in C++ disk-manager.cc)
 */
template<typename T>
class threadsafe_queue {
    static constexpr size_t interrupt_signal = static_cast<size_t>(-1);

    std::vector<std::optional<T>> buffer;
    size_t head;
    size_t tail;
    size_t count;
    size_t capacity;

    mutable std::mutex mtx;
    std::condition_variable cv_not_full;
    std::condition_variable cv_not_empty;

public:
    explicit threadsafe_queue(size_t capacity)
        : buffer(capacity), head(0), tail(0), count(0), capacity(capacity) {}

    void push(const T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_not_full.wait(lock, [this]() { return count < capacity; });

        buffer[tail] = value;
        tail = (tail + 1) % capacity;
        ++count;

        cv_not_empty.notify_one();
    }

    std::optional<T> pop(bool wait) {
        std::unique_lock<std::mutex> lock(mtx);
        if (!wait && count == 0) {
            return std::nullopt;
        }

        cv_not_empty.wait(lock, [this]() { return count > 0; });
        if (count == interrupt_signal) {
            return std::nullopt;
        }

        auto value = std::move(buffer[head]);
        buffer[head].reset();
        head = (head + 1) % capacity;
        --count;

        cv_not_full.notify_one();
        return value;
    }

    void interrupt() {
        {
            std::unique_lock lock(mtx);
            count = interrupt_signal;
        }

        cv_not_empty.notify_all();
    }

    void reset_interrupt_signal() {
        std::unique_lock lock(mtx);
        count = 0;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return count == 0;
    }

    size_t queue_size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }
};

#endif
