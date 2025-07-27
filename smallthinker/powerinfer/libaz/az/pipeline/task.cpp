#include "az/pipeline/task.hpp"

#include "az/assert.hpp"

namespace az {

void Task::add_prev_count(size_t count) {
    n_prev_tasks.fetch_add(count, std::memory_order_acq_rel);
}

void Task::link_to(const Arc<Task> &next_task) {
    next_task->add_prev_count(1);
    next_tasks.push_back(next_task);
}

void Task::on_launch() {
    AZ_DEBUG_ASSERT(target_queue) << [] { fmt::println("Call task_queue.schedule(task) first"); };
    target_queue->push_impl(shared_from_this());
}

void Task::on_finish() {
    for (auto &task : next_tasks) {
        task->on_prev_task_finished();
    }
    next_tasks.clear();

    AZ_DEBUG_ASSERT(target_queue);
    target_queue->n_unfinished_tasks.fetch_sub(1, std::memory_order_acq_rel);
}

void Task::on_prev_task_finished() {
    auto n = n_prev_tasks.fetch_sub(1, std::memory_order_acq_rel);
    AZ_DEBUG_ASSERT(n > 0);
    if (n == 1) {
        on_launch();
    }
}

}  // namespace az
