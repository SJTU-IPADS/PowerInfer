#include <cstring>
#include <numeric>

#include "powerinfer-log.hpp"
#include "convert.hpp"
#include "az/core/spin_barrier.hpp"
#include "az/core/cpu_affinity.hpp"
#include "az/core/perfetto_trace.hpp"
#include "moe_sparse_pipeline/config.hpp"
#include "moe_sparse_pipeline/pipeline.hpp"

#include <iostream>
#include <cstdio>
using az::global_spin_barrier;

namespace moe_sparse_pipeline {

Pipeline::Pipeline(
    ExpertCache &cache,
    size_t n_workers,
    size_t n_layers,
    size_t embed_dim,
    size_t ffn_hidden_dim,
    size_t max_batch_size,
    size_t n_experts,
    size_t n_used_experts,
    bool normalize_scores
) :
    cache(cache),
    cfg({
        .n_workers = n_workers,
        .n_layers = n_layers,
        .embed_dim = embed_dim,
        .ffn_hidden_dim = ffn_hidden_dim,
        .max_batch_size = max_batch_size,
        .n_experts = n_experts,
        .n_used_experts = n_used_experts,
        .normalize_scores = normalize_scores,

        .input_row_size = powerinfer_row_size(POWERINFER_TYPE_Q8_0, embed_dim),
        .up_row_size = powerinfer_row_size(POWERINFER_TYPE_Q4_0, embed_dim),
        .act_row_size = powerinfer_row_size(POWERINFER_TYPE_Q8_0, ffn_hidden_dim),
        .down_row_size = powerinfer_row_size(POWERINFER_TYPE_Q4_0, embed_dim),
    }),
    input_quant_buf(cfg.input_row_size * max_batch_size),
    local_out_bufs(n_workers),
    layers(n_layers)
{   
    // printf("!!! init pipeline: n_layers = %d, n_experts = %d, n_used_experts = %d, cfg.max_batch_size = %d\n", (int)n_layers, (int)n_experts, (int)n_used_experts, (int)cfg.max_batch_size);
    POWERINFER_ASSERT(cfg.max_batch_size <= moe_sparse_pipeline::max_batch_size);

    register_task_queue(&task_queue);
    set_n_threads(n_workers);

    workers.reserve(n_workers);
    for (size_t i = 0; i < n_workers; i++) {
        workers.emplace_back(std::make_unique<PipelineWorker>(task_queue));
        register_worker(workers[i].get(), i);
    }

    for (auto &buf : local_out_bufs) {
        buf.resize(cfg.max_batch_size * embed_dim);
    }
}

Pipeline::LayerData::LayerData(
    Pipeline *pipeline,
    size_t batch_size,
    const int32_t *expert_ids_data
) :
    pipeline(pipeline),
    cfg(pipeline->cfg),
    batch_size(batch_size)
{
    expert_ids.resize(batch_size);
    for (size_t i = 0; i < batch_size; i++) {
        const auto *row = &expert_ids_data[i * cfg.n_used_experts];
        expert_ids[i].assign(row, row + cfg.n_used_experts);
    }
}

void Pipeline::LayerData::process_expert_logits(const float *expert_logits) {
    expert_scores.resize(batch_size);
    for (auto &vec : expert_scores) {
        vec.resize(cfg.n_used_experts);
    }

    // Gather expert scores

    for (size_t i = 0; i < batch_size; i++) {
        for (size_t j = 0; j < cfg.n_used_experts; j++) {
            int32_t expert_id = expert_ids[i][j];
            expert_scores[i][j] = expert_logits[i * cfg.n_experts + expert_id];
        }
    }

    // Normalize scores if requested

    if (cfg.normalize_scores) {
        for (size_t  i = 0; i < batch_size; i++) {
            float sum = std::reduce(expert_scores[i].begin(), expert_scores[i].end(), 0.0f);
            float scale = 1.0f / sum;
            for (size_t j = 0; j < cfg.n_used_experts; j++) {
                expert_scores[i][j] *= scale;
            }
        }
    }
}

void Pipeline::prefetch(
    size_t layer_id,
    size_t batch_size,
    size_t n_predicted_experts,
    size_t max_n_prefetch,
    const int32_t *expert_ids
) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    POWERINFER_ASSERT(layer_id < cfg.n_layers);
    POWERINFER_ASSERT(batch_size <= cfg.max_batch_size);

    size_t n_prefetched = 0;
    for (size_t j = 0; j < n_predicted_experts; j++) {
        for (size_t i = 0; i < batch_size; i++) {
            int32_t expert_id = expert_ids[i * n_predicted_experts + j];

            for (size_t matrix_id = 0; matrix_id < N_MATRICES; matrix_id++) {
                if (n_prefetched >= max_n_prefetch) {
                    return;
                }

                auto &matrix = cache.get(layer_id, expert_id, matrix_id);

                std::unique_lock lock(*matrix.mutex);
                if (matrix.status == DATA_NOT_PRESENT) {
                    // printf("prefetch %d %d %d\t\t", (int)layer_id, (int)expert_id, (int)matrix_id);
                    cache.async_fetch(matrix);
                    cache.lru_promote(matrix);
                    n_prefetched++;
                    cache.stat.n_prefetched ++;
                }
            }
        }
    }
}

void Pipeline::build_tasks(
    size_t layer_id,
    size_t batch_size,
    int ffn_op_type,  // An ugly impl, 0 for RELU, 1 for SiLU
    const int32_t *expert_ids
) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    POWERINFER_ASSERT(layer_id < cfg.n_layers);
    POWERINFER_ASSERT(batch_size <= cfg.max_batch_size);
    POWERINFER_ASSERT(!layers[layer_id]);

    layers[layer_id] = std::make_unique<LayerData>(
        this,
        batch_size,
        expert_ids
    );
    auto &layer = *layers[layer_id];

    // Get how many times each expert is activated

    size_t activation_count[cfg.n_experts];
    memset(activation_count, 0, sizeof(activation_count));

    for (size_t i = 0; i < layer.batch_size; i++) {
        for (size_t j = 0; j < cfg.n_used_experts; j++) {
            int32_t expert_id = expert_ids[i * cfg.n_used_experts + j];
            activation_count[expert_id]++;
        }
    }

    // Sort expert ids in descending order

    size_t sorted_expert_ids[cfg.n_experts];
    std::iota(sorted_expert_ids, sorted_expert_ids + cfg.n_experts, 0);
    std::stable_sort(sorted_expert_ids, sorted_expert_ids + cfg.n_experts, [&](size_t i, size_t j) {
        return activation_count[i] > activation_count[j];
    });

    // Build tasks

    layer.preprocess_task = std::make_shared<PreprocessTask>(this, layer_id);
    auto reduce_task = std::make_shared<ReduceOutputTask>(this, layer_id);
    for (size_t expert_index = 0; expert_index < cfg.n_experts; expert_index++) {
        size_t expert_id = sorted_expert_ids[expert_index];
        if (activation_count[expert_id] == 0) {
            continue;
        }

        auto &up = cache.get(layer_id, expert_id, MATRIX_UP);
        auto &gate = cache.get(layer_id, expert_id, MATRIX_GATE);
        auto &down = cache.get(layer_id, expert_id, MATRIX_DOWN);

        // Build compute tasks for up, gate and down matrices

        std::vector<az::Arc<PipelineTask>> tasks[N_MATRICES];
        auto build_tasks = [&](size_t batch_id, size_t expert_index) {
            auto up_task = std::make_shared<UpForwardTask>(this, &up, batch_id);
            auto gate_task = std::make_shared<GateForwardTask>(this, &gate, batch_id, ffn_op_type);
            auto down_task = std::make_shared<DownForwardTask>(this, &down, batch_id, expert_index);

            layer.preprocess_task->link_to(gate_task);

            gate_task->link_to(up_task);
            up_task->link_to(down_task);
            down_task->link_to(reduce_task);

            up_task->gate_task = gate_task;
            down_task->up_task = up_task;
            down_task->gate_task = gate_task;

            tasks[MATRIX_UP].push_back(up_task);
            tasks[MATRIX_GATE].push_back(gate_task);
            tasks[MATRIX_DOWN].push_back(down_task);
        };

        for (size_t i = 0; i < layer.batch_size; i++) {
            for (size_t j = 0; j < cfg.n_used_experts; j++) {
                if (expert_ids[i * cfg.n_used_experts + j] == expert_id) {
                    build_tasks(i, j);
                    break;
                }
            }
        }

        // Submit IO requests for up, gate and down matrics

        size_t n_examined = 0;
        size_t n_cached = 0;
        for (size_t matrix_id = 0; matrix_id < N_MATRICES; matrix_id++) {
            n_examined++;

            auto &matrix = cache.get(layer_id, expert_id, matrix_id);

            std::unique_lock lock(*matrix.mutex);

            if (matrix.status == DATA_PRESENT) {
                n_cached++;
            } else {
                if (matrix.status == DATA_NOT_PRESENT) {
                    cache.async_fetch(matrix);
                }

                for (auto &task : tasks[matrix_id]) {
                    task->add_prev_count();
                    matrix.pending_tasks.emplace_back(task);
                }
            }

            cache.lru_promote(matrix);

            for (auto &task : tasks[matrix_id]) {
                task_queue.schedule(task);
            }
        }

        cache.stat.n_examined += n_examined;
        cache.stat.n_cached += n_cached;
    }

    // preprocess_task is delayed to forward
    task_queue.schedule(reduce_task);
}

void Pipeline::forward(
    HostComputeParam &param,
    size_t layer_id,
    const float *expert_logits,  // Shape: [batch_size, n_experts]
    const float *input,  // Shape: [batch_size, embed_dim]
    float *output  // Shape: [batch_size, embed_dim]
) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    az::auto_set_cpu_affinity(param.ith);

    POWERINFER_ASSERT(layers[layer_id]);
    auto &layer = layers[layer_id];

    if (param.ith == 0) {
        az::TraceEvent _("Pipeline::forward (preprocess)");

        layer->expert_logits = expert_logits;
        layer->input = input;
        layer->output = output;
        cparams.resize(param.nth);
        task_queue.schedule(layer->preprocess_task);
    }

    global_spin_barrier.wait();

    cparams[param.ith] = param;
    run(param.ith);

    global_spin_barrier.wait();

    if (param.ith == 0) {
        layer.reset();
    }
}

}

extern "C" {

static std::unique_ptr<moe_sparse_pipeline::Pipeline> global_moe_sparse_pipeline;

void powerinfer_init_moe_pipeline(
    size_t n_workers,
    size_t n_layers,
    size_t embed_dim,
    size_t ffn_hidden_dim,
    size_t batch_size,
    size_t n_experts,
    size_t n_used_experts,
    bool normalize_scores
) {
    global_moe_sparse_pipeline = std::make_unique<moe_sparse_pipeline::Pipeline>(
        moe_sparse_pipeline::get_global_expert_cache(),
        n_workers,
        n_layers,
        embed_dim,
        ffn_hidden_dim,
        batch_size,
        n_experts,
        n_used_experts,
        normalize_scores
    );
}

void powerinfer_moe_pipeline_prefetch(
    size_t layer_id,
    size_t batch_size,
    size_t n_predicted_experts,
    size_t max_n_prefetch,
    const int32_t *expert_ids
) {
    global_moe_sparse_pipeline->prefetch(layer_id, batch_size, n_predicted_experts, max_n_prefetch, expert_ids);
}

void powerinfer_moe_pipeline_build_tasks(
    size_t layer_id,
    size_t batch_size,
    int ffn_op_type,  // An ugly impl, 0 for RELU, 1 for SiLU
    const int32_t *expert_ids
) {
    global_moe_sparse_pipeline->build_tasks(layer_id, batch_size, ffn_op_type, expert_ids);
}

}

void powerinfer_moe_pipeline_forward(
    HostComputeParam &param,
    size_t layer_id,
    const float *expert_logits,
    const float *input,
    float *output
) {
    global_moe_sparse_pipeline->forward(param, layer_id, expert_logits, input, output);
}
