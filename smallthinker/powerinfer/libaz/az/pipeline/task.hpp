#pragma once

#include "az/assert.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

namespace az {

struct BaseTaskQueue;

struct Task : std::enable_shared_from_this<Task> {
    virtual ~Task() = default;

    void add_prev_count(size_t count = 1);
    void link_to(const Arc<Task> &next_task);

    /**
     * `on_finish` is manually called by the worker to notify the completion of the task.
     */
    void on_finish();

    /**
     * Invoked when all previous tasks are completed.
     */
    void on_launch();

    /**
     * Invoked when one of the previous task is completed.
     */
    void on_prev_task_finished();

private:
    template <typename T>
    friend struct TaskQueue;

    BaseTaskQueue *target_queue = nullptr;
    std::atomic<size_t> n_prev_tasks{1};  // Initial to one to wait for task_queue.schedule
    std::vector<Arc<Task>> next_tasks;
};

struct BaseTaskQueue {
    std::atomic<size_t> n_unfinished_tasks{0};

    virtual ~BaseTaskQueue() = default;

protected:
    friend struct Task;

    virtual void push_impl(Arc<Task> &&task) = 0;
    virtual auto pop_impl() -> Arc<Task> = 0;
};

template <typename T>
struct TaskQueue : BaseTaskQueue {
    static_assert(std::is_base_of_v<Task, T>, "Must be a subclass of Task");

    /**
     * `task` is scheduled on this queue, and the actual enqueuing is delayed until all prev tasks are finished.
     */
    template <typename U>
    void schedule(U *task) {
        static_assert(std::is_base_of_v<T, U>);
        AZ_DEBUG_ASSERT(task->target_queue == nullptr);

        task->target_queue = this;
        n_unfinished_tasks.fetch_add(1, std::memory_order_acq_rel);

        task->on_prev_task_finished();
    }

    template <typename U>
    void schedule(const Arc<U> &task) {
        schedule(task.get());
    }

    /**
     * If the queue is empty, it returns `nullptr`.
     */
    template <typename U = T>
    auto pop() -> Arc<U> {
        static_assert(std::is_base_of_v<T, U>);
        return std::static_pointer_cast<U>(pop_impl());
    }
};

template <typename T>
struct SimpleTaskQueue : TaskQueue<T> {
protected:
    std::mutex mutex;
    std::queue<Arc<Task>> queue;

    void push_impl(Arc<Task> &&task) override {
        std::unique_lock lock(mutex);
        queue.push(std::move(task));
    }

    auto pop_impl() -> Arc<Task> override {
        std::unique_lock lock(mutex);
        if (queue.empty()) {
            return nullptr;
        } else {
            auto task = std::move(queue.front());
            queue.pop();
            return task;
        }
    }
};

}  // namespace az
