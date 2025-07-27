#include "pipeline.hpp"

#include "az/assert.hpp"

#include <algorithm>

namespace az {

void Pipeline::register_task_queue(BaseTaskQueue *queue) {
    task_queues.push_back(queue);
}

void Pipeline::set_n_threads(size_t n_threads) {
    AZ_DEBUG_ASSERT(worker_map.empty());
    worker_map.resize(n_threads);
}

void Pipeline::register_worker(Worker *worker, size_t thread_id) {
    AZ_DEBUG_ASSERT(thread_id < worker_map.size());
    worker_map[thread_id].push_back(worker);
}

void Pipeline::run(size_t thread_id) {
    WorkerContext ctx = {
        .thread_id = thread_id,
    };

    while (true) {
        AZ_DEBUG_ASSERT(!task_queues.empty());

        bool flag = std::any_of(task_queues.begin(), task_queues.end(), [](BaseTaskQueue *queue) {
            return queue->n_unfinished_tasks.load(std::memory_order_relaxed) > 0;
        });

        if (!flag) {
            break;
        }

        AZ_DEBUG_ASSERT(thread_id < worker_map.size());
        for (auto worker : worker_map[thread_id]) {
            if (!worker->run_lock.try_lock()) {
                continue;
            }

            worker->run_tasks(ctx);

            worker->run_lock.unlock();
        }
    }
}

}  // namespace az
