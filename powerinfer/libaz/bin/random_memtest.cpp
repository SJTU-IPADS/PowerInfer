#include "CLI/CLI.hpp"
#include "az/core/utils.hpp"
#include "az/ggml/quant_types.hpp"
#include "az/ggml/vec_dot.hpp"

#include <atomic>
#include <barrier>
#include <random>
#include <sys/mman.h>

constexpr bool use_hugetlb = false;

int main(int argc, char *argv[]) {
    size_t n_rows = 65536;
    size_t row_dim = 2048;
    size_t n_threads = 1;
    size_t n_seconds = 60;
    float sparsity = 0.9;

    CLI::App app;
    app.add_option("--rows", n_rows);
    app.add_option("--dim", row_dim);
    app.add_option("--n-threads,-j", n_threads);
    app.add_option("--sparsity", sparsity);
    CLI11_PARSE(app, argc, argv);

    const size_t n_blocks = row_dim / az::ggml::block_q4_0::block_size;

    fmt::println("Row size={}", az::ggml::block_q4_0::row_size(row_dim));

    std::vector<float> float_data(row_dim);
    // std::vector<az::ggml::block_q4_0> weight_data(n_rows * n_blocks);

    const size_t total_blocks = n_rows * n_blocks;
    const size_t weight_size = total_blocks * sizeof(az::ggml::block_q4_0);

    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if constexpr (use_hugetlb) {
        fmt::println("Use hugetlb");
        flags |= MAP_HUGETLB | (25 << MAP_HUGE_SHIFT);  // 32 MiB hugepage
    }
    az::ggml::block_q4_0 *weight_data =
        (az::ggml::block_q4_0 *)mmap(nullptr, weight_size, PROT_READ | PROT_WRITE, flags, -1, 0);
    AZ_ASSERT(weight_data != MAP_FAILED);

    fmt::println("Weight data size: {:.3f} GiB", weight_size / 1024.0 / 1024 / 1024);

    std::mt19937 gen(19260817);
    std::uniform_real_distribution<float> dist(-0.1, 0.1);

    // 64: Reduce init time
    for (size_t i = 0; i < total_blocks; i += 64) {
        auto &block = weight_data[i];
        block.d = AZ_FP32_TO_FP16(dist(gen));
        for (auto q : block.qs) {
            q = gen();
        }
    }

    std::vector<az::ggml::block_q8_0> act_data(n_blocks);
    for (size_t j = 0; j < row_dim; j++) {
        float_data[j] = dist(gen);
    }
    az::ggml::quantize_row_q8_0(act_data.data(), float_data.data(), row_dim);

    fmt::println("Data prepared.");

    std::atomic<bool> flag{false};
    std::atomic<size_t> vec_dot_count{0};
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    std::barrier barrier(n_threads + 1);

    for (size_t i = 0; i < n_threads; i++) {
        threads.emplace_back([&, thread_id = i] {
            barrier.arrive_and_wait();

            std::mt19937 gen(233 * (thread_id + 1));
            std::uniform_real_distribution<float> dist(0, 1);

            size_t index = 0;
            float sum = 0;
            while (!flag.load(std::memory_order_acq_rel)) {
                if (dist(gen) >= sparsity) {
                    sum += az::ggml::vec_dot_q4_0_q8_0(row_dim, weight_data + i * n_blocks, act_data.data());
                    vec_dot_count.fetch_add(1, std::memory_order_acq_rel);
                }

                index = (index + 1) % n_rows;
            }

            fmt::println("sum={:.3f}", sum);
        });
    }

    barrier.arrive_and_wait();

    auto last_ts = az::Timer::now();
    for (size_t i = 0; i < n_seconds; i++) {
        sleep(1);

        auto cur_ts = az::Timer::now();
        size_t current_count = vec_dot_count.exchange(0);

        double tops = (current_count * row_dim * 2.0) / 1e3 / az::elapsed_ns(last_ts, cur_ts);
        fmt::println("#thread={}, count={}, {:.3} TOPS", n_threads, current_count, tops);

        last_ts = cur_ts;
    }

    flag = true;
    for (auto &t : threads) {
        t.join();
    }

    return 0;
}
