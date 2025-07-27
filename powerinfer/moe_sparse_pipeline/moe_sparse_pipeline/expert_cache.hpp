#pragma once

#include <mutex>
#include <string>
#include <memory>
#include <functional>

#include "powerinfer-disk-queue.hpp"
#include "moe_sparse_pipeline/iou.hpp"
#include "az/core/lru.hpp"
#include "az/pipeline/task.hpp"

namespace moe_sparse_pipeline {

enum MatrixType {
    MATRIX_UP = 0,
    MATRIX_GATE = 1,
    MATRIX_DOWN = 2,
    N_MATRICES,
};

using IOCallbackFn = std::function<void()>;

enum DataStatus {
    DATA_NOT_PRESENT,
    DATA_LOADING,
    DATA_PRESENT,
};

struct Matrix {
    size_t layer_id = 0;
    size_t expert_id = 0;
    size_t matrix_id = 0;
    size_t file_offset = 0;
    az::ListNode lru_node;

    char *data = nullptr;
    std::unique_ptr<std::mutex> mutex;
    DataStatus status = DATA_NOT_PRESENT;
    std::vector<az::Arc<az::Task>> pending_tasks;

    Matrix() : mutex(std::make_unique<std::mutex>()) {}
};

struct ExpertCache {
    struct {
        std::atomic<size_t> n_examined{0};
        std::atomic<size_t> n_cached{0};
        std::atomic<size_t> n_prefetched{0};
    } stat;

    ExpertCache(
        const std::string &expert_bundle_path,
        size_t n_layers,
        size_t n_experts,
        size_t n_matrices,
        size_t matrix_bytes
    );
    ~ExpertCache();

    auto get(size_t layer_id, size_t expert_id, size_t matrix_id) -> Matrix & {
        return layers[layer_id].experts[expert_id].matrices[matrix_id];
    }

    void lru_promote(Matrix &matrix);

    // NOTE: The caller should hold matrix's mutex
    void async_fetch(Matrix &matrix);

    void io_worker_main();

private:
    struct Expert {
        std::vector<Matrix> matrices;
    };

    struct Layer {
        std::vector<Expert> experts;
    };

    size_t n_layers = 0;
    size_t n_experts = 0;
    size_t n_matrices = 0;
    size_t matrix_bytes = 0;
    std::vector<Layer> layers;
    bool debug_print = false;

    IOUring iou;
    threadsafe_queue<Matrix *> io_queue;
    std::thread io_worker;

    az::LRU lru;

    void allocate_buffer(Matrix &matrix);
};

void set_global_expert_cache(const std::shared_ptr<ExpertCache> &cache_ptr);
bool has_global_expert_cache();
auto get_global_expert_cache() -> ExpertCache &;

}
