#pragma once

#include "powerinfer-cpu-param.hpp"
#include "moe_sparse_pipeline/expert_cache.hpp"
#include "moe_sparse_pipeline/task.hpp"
#include "az/pipeline/pipeline.hpp"

namespace moe_sparse_pipeline {

struct Pipeline : az::Pipeline {
    ExpertCache &cache;

    const struct Config {
        size_t n_workers = 0;
        size_t n_layers = 0;
        size_t embed_dim = 0;
        size_t ffn_hidden_dim = 0;
        size_t max_batch_size = 0;
        size_t n_experts = 0;
        size_t n_used_experts = 0;
        bool normalize_scores = false;

        size_t input_row_size = 0;
        size_t up_row_size = 0;
        size_t act_row_size = 0;
        size_t down_row_size = 0;
    } cfg;

    std::vector<char> input_quant_buf;
    std::vector<std::vector<float>> local_out_bufs;  // Shape: [n_workers, embed_dim]

    std::vector<HostComputeParam> cparams;
    az::SimpleTaskQueue<PipelineTask> task_queue;
    std::vector<std::unique_ptr<PipelineWorker>> workers;

    struct LayerData {
        Pipeline *pipeline = nullptr;
        const Pipeline::Config &cfg;
        size_t batch_size = 0;
        std::vector<std::vector<int32_t>> expert_ids;  // Shape: [batch_size, n_used_experts]
        const float *expert_logits = nullptr;
        const float *input = nullptr;
        float *output = nullptr;

        az::Arc<PreprocessTask> preprocess_task;
        std::vector<std::vector<float>> expert_scores;  // Shape: [batch_size, n_used_experts]

        LayerData(
            Pipeline *pipeline,
            size_t batch_size,
            const int32_t *expert_ids_data
        );

        void process_expert_logits(const float *expert_logits);
    };

    std::vector<std::unique_ptr<LayerData>> layers;

    Pipeline(
        ExpertCache &cache,
        size_t n_workers,
        size_t n_layers,
        size_t embed_dim,
        size_t ffn_hidden_dim,
        size_t max_batch_size,
        size_t n_experts,
        size_t n_used_experts,
        bool normalize_scores
    );

    void prefetch(
        size_t layer_id,
        size_t batch_size,
        size_t n_predicted_experts,
        size_t max_n_prefetch,
        const int32_t *expert_ids
);

    void build_tasks(
        size_t layer_id,
        size_t batch_size,
        int ffn_op_type,  // An ugly impl, 0 for RELU, 1 for SiLU
        const int32_t *expert_ids
    );

    void forward(
        HostComputeParam &param,
        size_t layer_id,
        const float *expert_logits,  // Shape: [batch_size, n_experts]
        const float *input,  // Shape: [batch_size, embed_dim]
        float *output  // Shape: [batch_size, embed_dim]
    );
};

}
