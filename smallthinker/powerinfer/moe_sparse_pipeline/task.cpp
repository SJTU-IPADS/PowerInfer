#include "convert.hpp"
#include "moe_sparse_pipeline/task.hpp"
#include "moe_sparse_pipeline/pipeline.hpp"
#include "moe_sparse_pipeline/packed_kernel.hpp"
#include "az/cpu/silu_lut.hpp"
#include "az/core/perfetto_trace.hpp"
#include "az/cpu/vec_dot.hpp"
#include "axpy.hpp"

namespace moe_sparse_pipeline {

PipelineWorker::PipelineWorker(az::TaskQueue<PipelineTask> &queue) : queue(&queue) {}

void PipelineWorker::run_tasks(const az::WorkerContext &ctx) {
    auto task = queue->pop();
    if (!task) {
        return;
    }

    task->run(task->pipeline->cparams[ctx.thread_id]);
    task->on_finish();
}

void ReduceOutputTask::run(HostComputeParam &param) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    AZ_UNUSED(param);

    auto &out_bufs = pipeline->local_out_bufs;
    auto *output = pipeline->layers[layer_id]->output;

    const size_t n = pipeline->cfg.embed_dim * pipeline->layers[layer_id]->batch_size;
    memcpy(output, out_bufs[0].data(), sizeof(float) * n);

    for (size_t i = 1; i < param.nth; i++) {
        for (size_t j = 0; j < n; j++) {
            output[j] += out_bufs[i][j];
        }
    }
}

void PreprocessTask::run(HostComputeParam &param) {
    az::TraceEvent _(__PRETTY_FUNCTION__);
    AZ_UNUSED(param);

    auto &layer = pipeline->layers[layer_id];
    const auto &cfg = pipeline->cfg;

    for (auto &buf : pipeline->local_out_bufs) {
        memset(buf.data(), 0, sizeof(buf[0]) * buf.size());
    }

    // Quantize input to q8_0
    for (size_t i = 0; i < layer->batch_size; i++) {
        quantize_row_q8_0(layer->input + i * cfg.embed_dim, pipeline->input_quant_buf.data() + i * cfg.input_row_size, cfg.embed_dim);
    }

    layer->process_expert_logits(layer->expert_logits);
}

void UpForwardTask::run(HostComputeParam &param) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    AZ_UNUSED(param);

    const auto &cfg = pipeline->cfg;
    POWERINFER_ASSERT(cfg.ffn_hidden_dim % vec_dot_block_size == 0);

    out = std::make_unique<float[]>(cfg.ffn_hidden_dim);

    for (size_t j = 0; j < cfg.ffn_hidden_dim; j++) {
        if(gate_task->out[j] <= 0.0f){
            out[j] = 0.0f;
        }
        else{
            out[j] = az::cpu::vec_dot_q4_0_q8_0(cfg.embed_dim, 
                (az::cpu::block_q4_0 *)(matrix->data + j * cfg.up_row_size), 
                (az::cpu::block_q8_0 *)(pipeline->input_quant_buf.data() + batch_id * cfg.input_row_size)
            );
        }
    }
}

void GateForwardTask::run(HostComputeParam &param) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    AZ_UNUSED(param);

    const auto &cfg = pipeline->cfg;
    out = std::make_unique<float[]>(cfg.ffn_hidden_dim);
#ifdef __ARM_ARCH
    float tmp[4];
    for (size_t j = 0; j < cfg.ffn_hidden_dim; j += vec_dot_block_size) {
        gemv_q4_0_4x4_q8_0(
            cfg.embed_dim,
            tmp,
            matrix->data + j * cfg.up_row_size,
            pipeline->input_quant_buf.data() + batch_id * cfg.input_row_size,
            vec_dot_block_size
        );    

        // RELU
        for (size_t k = 0; k < vec_dot_block_size; k++) {
            out[j + k] = std::fmax(0.0f, tmp[k]);
        }
    }
#else 
    for (size_t j = 0; j < cfg.ffn_hidden_dim; j++) {
        out[j] = az::cpu::vec_dot_q4_0_q8_0(cfg.embed_dim, 
            (az::cpu::block_q4_0 *)(matrix->data + j * cfg.up_row_size), 
            (az::cpu::block_q8_0 *)(pipeline->input_quant_buf.data() + batch_id * cfg.input_row_size)
        );
        out[j] = std::fmax(0.0f,out[j]);
    }
#endif
}

void DownForwardTask::run(HostComputeParam &param) {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    const auto &cfg = pipeline->cfg;
    POWERINFER_ASSERT(cfg.embed_dim % vec_dot_block_size == 0);

    std::unique_ptr<float []> tmp = std::make_unique<float[]>(cfg.embed_dim);
    AxpyBatch axpy(cfg.embed_dim, tmp.get());

    for (size_t j = 0; j < cfg.ffn_hidden_dim; j++) {
        up_task->out[j] *= gate_task->out[j];
        if(up_task->out[j])
        {
            axpy.enqueue(up_task->out[j],matrix->data + j * cfg.down_row_size);
        }
    }
    axpy.flush();

    // act_quant_buf = std::make_unique<char[]>(cfg.act_row_size);

    // quantize_row_q8_0(up_task->out.get(), act_quant_buf.get(), cfg.ffn_hidden_dim);



    const float score = pipeline->layers[matrix->layer_id]->expert_scores[batch_id][expert_index];
    
    
    // for (size_t j = 0; j < cfg.embed_dim; j ++) {
    //     out[j] = score * az::cpu::vec_dot_q4_0_q8_0(
    //         cfg.ffn_hidden_dim, 
    //             (az::cpu::block_q4_0 *)(matrix->data + j * cfg.down_row_size), 
    //             (az::cpu::block_q8_0 *)(act_quant_buf.get())
    //     );
    // }
    float *out = pipeline->local_out_bufs[param.ith].data() + batch_id * cfg.embed_dim;
    for (size_t j = 0; j < cfg.embed_dim; j ++) {
        out[j] += score * tmp[j];
    }
}

}
