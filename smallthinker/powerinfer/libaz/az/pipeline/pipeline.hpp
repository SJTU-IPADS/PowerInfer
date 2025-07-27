#pragma once

#include "az/pipeline/task.hpp"
#include "az/pipeline/worker.hpp"

namespace az {

struct Pipeline {
    virtual ~Pipeline() = default;

    std::vector<std::vector<Worker *>> worker_map;
    std::vector<BaseTaskQueue *> task_queues;

    void register_task_queue(BaseTaskQueue *queue);
    void set_n_threads(size_t n_threads);
    void register_worker(Worker *worker, size_t thread_id);

    void run(size_t thread_id);
};

}  // namespace az
