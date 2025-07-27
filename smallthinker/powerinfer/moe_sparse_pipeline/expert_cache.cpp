#include <fcntl.h>

#include "powerinfer-perf.hpp"
#include "powerinfer-log.hpp"
#include "moe_sparse_pipeline/config.hpp"
#include "moe_sparse_pipeline/expert_cache.hpp"
#include "az/core/perfetto_trace.hpp"

namespace moe_sparse_pipeline {

ExpertCache::ExpertCache(
    const std::string &expert_bundle_path,
    size_t n_layers,
    size_t n_experts,
    size_t n_matrices,
    size_t matrix_bytes) :
    iou(expert_bundle_path, io_queue_depth),
    io_queue(io_queue_depth),
    n_layers(n_layers),
    n_experts(n_experts),
    n_matrices(n_matrices),
    matrix_bytes(matrix_bytes)
{
    POWERINFER_ASSERT(matrix_bytes % io_alignment == 0);

    layers.resize(n_layers);
    for (size_t layer_id = 0; layer_id < n_layers; layer_id++) {
        auto &layer = layers[layer_id];
        layer.experts.resize(n_experts);

        for (size_t expert_id = 0; expert_id < n_experts; expert_id++) {
            auto &expert = layer.experts[expert_id];
            expert.matrices.resize(n_matrices);

            for (size_t matrix_id = 0; matrix_id < n_matrices; matrix_id++) {
                auto &matrix = expert.matrices[matrix_id];
                matrix.layer_id = layer_id;
                matrix.expert_id = expert_id;
                matrix.matrix_id = matrix_id;
                matrix.file_offset = matrix_bytes * (matrix_id + expert_id * n_matrices + layer_id * n_experts * n_matrices);
            }
        }
    }

    io_worker = std::thread(&ExpertCache::io_worker_main, this);
}

static Matrix *const io_worker_exit_signal = reinterpret_cast<Matrix *>(-1llu);

ExpertCache::~ExpertCache() {
    io_queue.push(io_worker_exit_signal);
    io_worker.join();

    while (lru.size > 0) {
        Matrix *victim = lru.evict().get_owner_ptr(&Matrix::lru_node);
        free(victim->data);
        victim->data = nullptr;
    }

    if(debug_print){
        printf(
            "Expert cache: #examined=%zu, #cached=%zu (%.2lf%%)\n",
            stat.n_examined.load(),
            stat.n_cached.load(),
            100.0 * stat.n_cached / stat.n_examined
        );

        printf(
            "Expert cache: #examined=%zu, #prefetched=%zu (%.2lf%%)\n",
            stat.n_examined.load(),
            stat.n_prefetched.load(),
            100.0 * stat.n_prefetched / stat.n_examined
        );
    }
}

void ExpertCache::lru_promote(Matrix &matrix) {
    lru.promote(matrix.lru_node);
}

void ExpertCache::async_fetch(Matrix &matrix) {
    az::TraceEvent _("launch_io");

    POWERINFER_ASSERT(matrix.status == DATA_NOT_PRESENT);
    matrix.status = DATA_LOADING;
    allocate_buffer(matrix);
    io_queue.push(&matrix);
}

void ExpertCache::io_worker_main() {
#if defined(__linux__)
    if (io_worker_cpu_affinity != -1) {
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(io_worker_cpu_affinity, &cpus);
        int ret = sched_setaffinity(0, sizeof(cpus), &cpus);
        POWERINFER_ASSERT(ret >= 0);
    }
#endif

    // NOTE: Do not manipulate LRU in IO worker

    while (true) {
        powerinfer_begin_event("io_queue.pop");
        Matrix *matrix = nullptr;
        auto v = io_queue.pop(iou.n_inflight <= 0);
        if (v.has_value()) {
            matrix = v.value();
        }
        powerinfer_end_event();

        if (matrix == io_worker_exit_signal) {
            break;
        }

        bool any_submit = false;
        if (matrix) {
            POWERINFER_ASSERT(matrix->data);
            iou.enqueue_read(matrix->data, matrix->file_offset, matrix_bytes, matrix, [](void *user_data) {
                Matrix *matrix = static_cast<Matrix *>(user_data);

                std::unique_lock lock(*matrix->mutex);
                matrix->status = DATA_PRESENT;
                for (auto &task : matrix->pending_tasks) {
                    task->on_prev_task_finished();
                }
                matrix->pending_tasks.clear();
            });

            any_submit = true;
        }

        // If no IO request submitted, we wait for previous requests to complete
        if (!any_submit) {
            powerinfer_begin_event("iou.submit_and_wait");
            size_t wait_nr = iou.n_inflight > 0 ? 1 : 0;
            iou.submit_and_wait(wait_nr);
            powerinfer_end_event();
        }

        iou.reap();
    }
}

void ExpertCache::allocate_buffer(Matrix &matrix) {
    POWERINFER_ASSERT(!matrix.data);

    if (lru.size < max_n_cached_matrices) {
        matrix.data = static_cast<char *>(aligned_alloc(io_alignment, matrix_bytes));
    } else {
        Matrix *victim = lru.evict().get_owner_ptr(&Matrix::lru_node);

        std::unique_lock lock(*victim->mutex);
        matrix.data = victim->data;
        victim->status = DATA_NOT_PRESENT;
        victim->data = nullptr;
    }

    lru.add(matrix.lru_node);
}

static std::shared_ptr<ExpertCache> cache_ptr{nullptr};

void set_global_expert_cache(const std::shared_ptr<ExpertCache> &cache_ptr) {
    ::moe_sparse_pipeline::cache_ptr = cache_ptr;
}

bool has_global_expert_cache() {
    return ::moe_sparse_pipeline::cache_ptr != nullptr;
}

auto get_global_expert_cache() -> ExpertCache & {
    return *::moe_sparse_pipeline::cache_ptr;
}

}

extern "C" {

void powerinfer_init_global_expert_cache(
    const char *expert_bundle_path,
    size_t n_layers,
    size_t n_experts,
    size_t n_matrices,
    size_t matrix_bytes
) {
    moe_sparse_pipeline::set_global_expert_cache(std::make_shared<moe_sparse_pipeline::ExpertCache>(
        expert_bundle_path,
        n_layers,
        n_experts,
        n_matrices,
        matrix_bytes
    ));
}

bool powerinfer_has_global_expert_cache(void) {
    return moe_sparse_pipeline::has_global_expert_cache();
}

}
