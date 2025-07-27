#pragma once

#include "az/core/spin_lock.hpp"

namespace az {

struct WorkerContext {
    size_t thread_id;
};

struct Worker {
    SpinLock run_lock;

    virtual ~Worker() = default;
    virtual void run_tasks(const WorkerContext &ctx) = 0;
};

}  // namespace az
