#include <atomic>
#include <chrono>
#include <mutex>
#include <cassert>

#include "az/core/spin_barrier.hpp"
#include "fused_sparse_moe/fused_sparse_moe.hpp"
#include "axpy.hpp"
#include "convert.hpp"
#include "az/cpu/vec_dot.hpp"
#include "az/core/perfetto_trace.hpp"
#include "powerinfer-exception.hpp"
#include "az/cpu/aarch64/gemv.hpp"

using az::global_spin_barrier;

// TODO: Fix it
#define SIGMOID_ROUTER_BLOCK_SIZE 16

namespace fused_sparse_moe {

constexpr bool use_drelu = false;
constexpr bool no_sparsity = false;
constexpr size_t max_n_threads = 8;
constexpr size_t max_embed_dim = 4096;
constexpr size_t forward_chunk_size = 256;
constexpr bool track_kernel_time = false;

struct Timer {
    using Clock = std::chrono::steady_clock;

    Clock::time_point last_ts;
    size_t up_time = 0;
    size_t gate_time = 0;
    size_t down_time = 0;

    void start() {
        if constexpr (track_kernel_time) {
            last_ts = Clock::now();
        }
    }

    void end(size_t &dst) const {
        if constexpr (track_kernel_time) {
            auto cur_ts = Clock::now();
            dst += std::chrono::nanoseconds(cur_ts - last_ts).count();
        }
    }
};

// TODO: Remove global states
struct GlobalStates {
    axpy_out_type local_buf[max_n_threads][max_embed_dim];
    std::atomic<size_t> current_neuron{0};
    std::atomic<size_t> nr_examined{0};
    std::atomic<size_t> nr_predicted{0};
    std::atomic<size_t> nr_activated{0};
    std::atomic<size_t> up_time{0};
    std::atomic<size_t> gate_time{0};
    std::atomic<size_t> down_time{0};

    ~GlobalStates() {
        printf(
            "fused_sparse_moe: #examined=%zu, #predicted=%zu (%.2f%%), #activated=%zu (%.2f%%)\n",
            nr_examined.load(),
            nr_predicted.load(), 100.0 * nr_predicted / nr_examined,
            nr_activated.load(), 100.0 * nr_activated / nr_examined
        );

        if constexpr (track_kernel_time) {
            printf("up: %.3lf us, gate: %.3lf us, down: %.3lf us\n", up_time / 1e3, gate_time / 1e3, down_time / 1e3);
        }
    }
};

static std::once_flag global_states_once_flag;
static std::unique_ptr<GlobalStates> global_states_ptr;

auto get_global_states() -> GlobalStates & {
    std::call_once(global_states_once_flag, [&] {
        global_states_ptr = std::make_unique<GlobalStates>();
    });

    return *global_states_ptr;
}

template <size_t batch_size>
struct FusedSparseFFNImpl {
    const HostComputeParam &param;
    size_t embed_dim = 0;
    size_t ffn_hidden_dim = 0;
    const char *up_weight = nullptr;
    const char *gate_weight = nullptr;
    const char *down_weight = nullptr;
    const char *quantized_act = nullptr;
    const float *router_out = nullptr;
    float *output = nullptr;

    void forward_chunk(AxpyBatch &axpy, size_t beg, size_t end);
    void forward(bool do_accumulate=false,float scale=1.0);
};

template <>
void FusedSparseFFNImpl<1>::forward_chunk(AxpyBatch &axpy, size_t beg, size_t end) {
    Timer timer;

    auto &global_states = get_global_states();
    axpy_out_type *local_buf = global_states.local_buf[param.ith];

    size_t weight_row_size = powerinfer_row_size(POWERINFER_TYPE_Q4_0, embed_dim);
   
    size_t nr_examined = 0;
    size_t nr_predicted = 0;
    size_t nr_activated = 0;
    float gate_out[SIGMOID_ROUTER_BLOCK_SIZE];
    bool activated[SIGMOID_ROUTER_BLOCK_SIZE];
    for (size_t i = beg; i < end; i += SIGMOID_ROUTER_BLOCK_SIZE) {
        nr_examined += SIGMOID_ROUTER_BLOCK_SIZE;

        if (router_out) {
            float router = router_out[i / SIGMOID_ROUTER_BLOCK_SIZE];
            if (router <= 0) {
                continue;
            }
        }

        nr_predicted += SIGMOID_ROUTER_BLOCK_SIZE;

        timer.start();
        for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
            const char *gate_row = gate_weight + (i + j) * weight_row_size;
            gate_out[j] = az::cpu::vec_dot_q4_0_q8_0 (embed_dim, gate_row, quantized_act);
            if (gate_out[j] <= 0) {
                gate_out[j] = 0;
            }
        }
        timer.end(timer.gate_time);

        if constexpr (no_sparsity) {
            for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
                activated[j] = true;
            }
        } else {
            for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
                activated[j] = (gate_out[j] > 0);
            }

            // for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j += 4) {
            //     bool flag = (gate_out[j] > 0 || gate_out[j + 1] > 0 || gate_out[j + 2] > 0 || gate_out[j + 3] > 0);
            //     activated[j] = flag;
            //     activated[j + 1] = flag;
            //     activated[j + 2] = flag;
            //     activated[j + 3] = flag;
            // }
        }

        timer.start();
        for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
            if (!activated[j]) {
                continue;
            }

            const char *up_row = up_weight + (i + j) * weight_row_size;
            float up;

            up = az::cpu::vec_dot_q4_0_q8_0 (embed_dim, up_row, quantized_act);
            gate_out[j] *= up;

            activated[j] = true;
            nr_activated++;
        }
        timer.end(timer.up_time);

        static_assert(!use_drelu);

        timer.start();
        for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
            if (!activated[j]) {
                continue;
            }

            const char *down_row = down_weight + (i + j) * weight_row_size;
            axpy.enqueue(gate_out[j], down_row);
        }
        timer.end(timer.down_time);
    }

    global_states.nr_examined += nr_examined;
    global_states.nr_predicted += nr_predicted;
    global_states.nr_activated += nr_activated;

    if constexpr (track_kernel_time) {
        global_states.up_time += timer.up_time;
        global_states.gate_time += timer.gate_time;
        global_states.down_time += timer.down_time;
    }
}

template <>
void FusedSparseFFNImpl<1>::forward(bool do_accumulate,float scale) {
    size_t ith = param.ith;
    size_t nth = param.nth;

    auto &global_states = get_global_states();

    if (ith == 0) {
        global_states.current_neuron = 0;
    }

    axpy_out_type *local_buf = global_states.local_buf[ith];
    memset(local_buf, 0, sizeof(axpy_out_type) * embed_dim);

    global_spin_barrier.wait();

    AxpyBatch axpy(embed_dim, local_buf);

    while (true) {
        size_t beg = global_states.current_neuron.fetch_add(forward_chunk_size, std::memory_order_relaxed);
        if (beg >= ffn_hidden_dim) {
            break;
        }

        size_t end = std::min(ffn_hidden_dim, beg + forward_chunk_size);
        forward_chunk(axpy, beg, end);
    }

    axpy.flush();

    global_spin_barrier.wait();

    if (ith == 0) {
        const axpy_out_type *ffn_outs[nth];
        for (size_t i = 0; i < nth; i++) {
            ffn_outs[i] = global_states.local_buf[i];
        }

        axpy_reduce_f32(embed_dim, nth, ffn_outs, output,do_accumulate,scale);
    }

    global_spin_barrier.wait();
}

template <size_t batch_size,typename BLOC_TYPE, int64_t INTER_SIZE, int64_t NB_COLS>
struct FusedSparseRepackFFNImpl {
    const HostComputeParam &param;
    size_t embed_dim = 0;
    size_t ffn_hidden_dim = 0;
    const char *up_weight = nullptr;
    const char *gate_weight = nullptr;
    const char *down_weight = nullptr;
    const char *quantized_act = nullptr;
    const float *router_out = nullptr;
    float *output = nullptr;
    
    void forward_chunk(AxpyBatch &axpy, size_t beg, size_t end);
    void forward(bool do_accumulate=false,float scale=1.0);
};

template <typename BLOC_TYPE, int64_t INTER_SIZE, int64_t NB_COLS>
struct FusedSparseRepackFFNImpl<1,BLOC_TYPE,INTER_SIZE,NB_COLS>
{
    const HostComputeParam &param;
    size_t embed_dim = 0;
    size_t ffn_hidden_dim = 0;
    const char *up_weight = nullptr;
    const char *gate_weight = nullptr;
    const char *down_weight = nullptr;
    const char *quantized_act = nullptr;
    const float *router_out = nullptr;
    float *output = nullptr;

    void forward_chunk(AxpyBatch &axpy, size_t beg, size_t end) {
        Timer timer;

        auto &global_states = get_global_states();
        axpy_out_type *local_buf = global_states.local_buf[param.ith];

        size_t weight_row_size = powerinfer_row_size(POWERINFER_TYPE_Q4_0, embed_dim);
        

        size_t nr_examined = 0;
        size_t nr_predicted = 0;
        size_t nr_activated = 0;
        float gate_out[SIGMOID_ROUTER_BLOCK_SIZE];
        bool activated[SIGMOID_ROUTER_BLOCK_SIZE];
        for (size_t i = beg; i < end; i += SIGMOID_ROUTER_BLOCK_SIZE) {
            nr_examined += SIGMOID_ROUTER_BLOCK_SIZE;

            if (router_out) {
                float router = router_out[i / SIGMOID_ROUTER_BLOCK_SIZE];
                if (router <= 0) {
                    continue;
                }
            }

            nr_predicted += SIGMOID_ROUTER_BLOCK_SIZE;

            timer.start();
            for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j+=NB_COLS) {
                const char *gate_rows = gate_weight + (i + j) * weight_row_size;
                //gate_out[j] = az::cpu::vec_dot_q4_0_q8_0(embed_dim, (az::cpu::block_q4_0 *)gate_rows, (az::cpu::block_q8_0 *)quantized_act);
                az::cpu::gemv<BLOC_TYPE,INTER_SIZE,NB_COLS>(embed_dim,gate_out+j,0,gate_rows,quantized_act,1,NB_COLS);
                for(int k=j;k<j+NB_COLS;k++)
                {
                    if (gate_out[k] <= 0) {
                        gate_out[k] = 0;
                    }
                }
                // if(gate_out[j]<=0)
                // {
                //     gate_out[j]=0;
                // }
            }
            timer.end(timer.gate_time);

            if constexpr (no_sparsity) {
                for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
                    activated[j] = true;
                }
            } else {
                for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
                    activated[j] = (gate_out[j] > 0);
                }

                // for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j += 4) {
                //     bool flag = (gate_out[j] > 0 || gate_out[j + 1] > 0 || gate_out[j + 2] > 0 || gate_out[j + 3] > 0);
                //     activated[j] = flag;
                //     activated[j + 1] = flag;
                //     activated[j + 2] = flag;
                //     activated[j + 3] = flag;
                // }
            }

            timer.start();
            for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j+=4) {
                if (!activated[j]&&!activated[j+1]&&!activated[j+2]&&!activated[j+3]) {
                    continue;
                }

                const char *up_row = up_weight + (i + j) * weight_row_size;
                float up[4];

                //up = vec_dot_fun(embed_dim, up_row, quantized_act);
                az::cpu::gemv<BLOC_TYPE,INTER_SIZE,NB_COLS>(embed_dim,up,0,up_row,quantized_act,1,NB_COLS);
                for(int k=0;k<4;k++)
                {
                    if(activated[j+k])
                    {
                        gate_out[j+k] *= up[k];

                    activated[j+k] = true;
                    nr_activated++;
                    }
                }
                // gate_out[j]*=up;
                // activated[j]=true;
                // nr_activated++;
                
            }
            timer.end(timer.up_time);

            static_assert(!use_drelu);

            timer.start();
            for (size_t j = 0; j < SIGMOID_ROUTER_BLOCK_SIZE; j++) {
                if (!activated[j]) {
                    continue;
                }
                const char *down_row = down_weight + (i + j) * weight_row_size;
                axpy.enqueue(gate_out[j], down_row);
            }
            timer.end(timer.down_time);
        }

        global_states.nr_examined += nr_examined;
        global_states.nr_predicted += nr_predicted;
        global_states.nr_activated += nr_activated;

        if constexpr (track_kernel_time) {
            global_states.up_time += timer.up_time;
            global_states.gate_time += timer.gate_time;
            global_states.down_time += timer.down_time;
        }
}
    
    void forward(bool do_accumulate=false,float scale=1.0) {
        size_t ith = param.ith;
        size_t nth = param.nth;

        auto &global_states = get_global_states();

        if (ith == 0) {
            global_states.current_neuron = 0;
        }

        axpy_out_type *local_buf = global_states.local_buf[ith];
        memset(local_buf, 0, sizeof(axpy_out_type) * embed_dim);

        global_spin_barrier.wait();

        AxpyBatch axpy(embed_dim, local_buf);

        while (true) {
            size_t beg = global_states.current_neuron.fetch_add(forward_chunk_size, std::memory_order_relaxed);
            if (beg >= ffn_hidden_dim) {
                break;
            }

            size_t end = std::min(ffn_hidden_dim, beg + forward_chunk_size);
            forward_chunk(axpy, beg, end);
        }

        axpy.flush();

        global_spin_barrier.wait();

        if (ith == 0) {
            const axpy_out_type *ffn_outs[nth];
            for (size_t i = 0; i < nth; i++) {
                ffn_outs[i] = global_states.local_buf[i];
            }

            axpy_reduce_f32(embed_dim, nth, ffn_outs, output,do_accumulate,scale);
        }

        global_spin_barrier.wait();
    }
};


void FusedSparseMoE::forward() {
    az::TraceEvent _(__PRETTY_FUNCTION__);

    assert(param.nth <= max_n_threads);
    assert(embed_dim <= max_embed_dim);
    
    size_t act_row_size= powerinfer_row_size(POWERINFER_TYPE_Q8_0, embed_dim);
    size_t expert_size=powerinfer_row_size(POWERINFER_TYPE_Q4_0,embed_dim*ffn_hidden_dim);

    if (param.ith == 0) {
        for (size_t i = 0; i < batch_size; i++) {
            quantize_row_q8_0(activation + i * embed_dim, wdata + i * act_row_size, embed_dim);
        }
    }
    // global_spin_barrier.wait();



#if defined(__ARM_NEON) && (defined(__ARM_FEATURE_MATMUL_INT8) || defined(__ARM_FEATURE_DOTPROD))
    auto run_fun = [&] <typename BlockT> {
        for (size_t i = 0; i < batch_size; i++) {
            for (size_t j = 0; j < n_expert_used; j++) {
                const int32_t expert_idx = selected_experts[i * n_expert_used + j];

                const char* current_up_weight = up_weight + expert_idx * expert_size;
                const char* current_gate_weight = gate_weight + expert_idx * expert_size;
                const char* current_down_weight =down_weight + expert_idx * expert_size;
                char* quantized_act = static_cast<char*>(wdata) + i * act_row_size;
                float* current_output = output + i * embed_dim;

#if defined(__ARM_FEATURE_MATMUL_INT8)
                FusedSparseRepackFFNImpl<1, BlockT, 8, 4> impl = {
                    .param = param, 
                    .embed_dim = embed_dim, 
                    .ffn_hidden_dim = ffn_hidden_dim,
                    .up_weight = current_up_weight, 
                    .gate_weight = current_gate_weight, 
                    .down_weight = current_down_weight,
                    .quantized_act = quantized_act, 
                    .output = current_output
                };
                impl.forward(j != 0, expert_weights[j]);
#elif defined(__ARM_FEATURE_DOTPROD)
                FusedSparseRepackFFNImpl<1, BlockT, 4, 4> impl = {
                    .param = param, 
                    .embed_dim = embed_dim, 
                    .ffn_hidden_dim = ffn_hidden_dim,
                    .up_weight = current_up_weight, 
                    .gate_weight = current_gate_weight, 
                    .down_weight = current_down_weight,
                    .quantized_act = quantized_act, 
                    .output = current_output
                };
                impl.forward(j != 0, expert_weights[j]);
#endif
            }
        }
    };

    run_fun.template operator()<az::cpu::block_q4_0>();

#else
    for (size_t i = 0; i < batch_size; i++) {
        for (size_t j = 0; j < n_expert_used; j++) {
            int32_t expert_idx = selected_experts[i * n_expert_used + j];
            
            FusedSparseFFNImpl<1> impl = {
                .param = param,
                .embed_dim = embed_dim,
                .ffn_hidden_dim = ffn_hidden_dim,
                .up_weight = up_weight + expert_idx * expert_size,
                .gate_weight = gate_weight + expert_idx * expert_size,
                .down_weight = down_weight + expert_idx * expert_size
            };
            impl.quantized_act = wdata + i * act_row_size;
            impl.output = output + i * embed_dim;
            impl.forward(j != 0, expert_weights[j]);
        }
    }
#endif
}

}

PowerInferError powerinfer_host_fused_sparse_moe_impl(
    HostComputeParam param,
    size_t batch_size,
    size_t embed_dim,
    size_t ffn_hidden_dim,
    size_t n_expert_used,
    const char *up_weight,
    const char *gate_weight,
    const char *down_weight,
    const float *activation,
    const int32_t *selected_experts,
    const float* expert_weights,
    float *output,
    char *wdata
) {
    fused_sparse_moe::FusedSparseMoE{
        .param = param,
        .batch_size = batch_size,
        .embed_dim = embed_dim,
        .ffn_hidden_dim = ffn_hidden_dim,
        .n_expert_used=n_expert_used,
        .up_weight = up_weight,
        .gate_weight = gate_weight,
        .down_weight = down_weight,
        .activation = activation,
        .selected_experts = selected_experts,
        .expert_weights=expert_weights,
        .output = output,
        .wdata = wdata
    }.forward();

    return { false, "Success" };
}