#pragma once

#include <memory>

#include "powerinfer-cpu-param.hpp"
#include "az/pipeline/task.hpp"
#include "az/pipeline/worker.hpp"

namespace moe_sparse_pipeline {

struct Pipeline;

struct PipelineTask : az::Task {
    Pipeline *const pipeline = nullptr;

    explicit PipelineTask(Pipeline *pipeline) : pipeline(pipeline) {}

    virtual void run(HostComputeParam &param) = 0;
};

struct PipelineWorker : az::Worker {
    az::TaskQueue<PipelineTask> *queue = nullptr;

    explicit PipelineWorker(az::TaskQueue<PipelineTask> &queue);

    void run_tasks(const az::WorkerContext &ctx) override;
};

struct ReduceOutputTask : PipelineTask {
    size_t layer_id;

    ReduceOutputTask(Pipeline *pipeline, size_t layer_id) : PipelineTask(pipeline), layer_id(layer_id) {}

    void run(HostComputeParam &param) override;
};

struct PreprocessTask : PipelineTask {
    size_t layer_id;

    PreprocessTask(Pipeline *pipeline, size_t layer_id) : PipelineTask(pipeline), layer_id(layer_id) {}

    void run(HostComputeParam &param) override;
};

struct Matrix;

struct ForwardTask : PipelineTask {
    Matrix *const matrix = nullptr;
    const size_t batch_id = 0;

    ForwardTask(Pipeline *pipeline, Matrix *matrix, size_t batch_id) :
        PipelineTask(pipeline),
        matrix(matrix),
        batch_id(batch_id) {}
};

struct GateForwardTask : ForwardTask {
    std::unique_ptr<float[]> out;
    int ffn_op_type;  // An ugly impl, 0 for RELU, 1 for SiLU
    GateForwardTask(Pipeline *pipeline, Matrix *matrix, size_t batch_id, int ffn_op_type) :
        ForwardTask(pipeline, matrix, batch_id), ffn_op_type(ffn_op_type) {}

    void run(HostComputeParam &param) override;
};
struct UpForwardTask : ForwardTask {
    std::unique_ptr<float[]> out;
    std::shared_ptr<GateForwardTask> gate_task;

    UpForwardTask(Pipeline *pipeline, Matrix *matrix, size_t batch_id) :
        ForwardTask(pipeline, matrix, batch_id) {}

    void run(HostComputeParam &param) override;
};

struct DownForwardTask : ForwardTask {
    size_t expert_index = 0;
    std::unique_ptr<char[]> act_quant_buf;
    std::shared_ptr<UpForwardTask> up_task;
    std::shared_ptr<GateForwardTask> gate_task;

    DownForwardTask(Pipeline *pipeline, Matrix *matrix, size_t batch_id, size_t expert_index) :
        ForwardTask(pipeline, matrix, batch_id),
        expert_index(expert_index) {}

    void run(HostComputeParam &param) override;
};

}
