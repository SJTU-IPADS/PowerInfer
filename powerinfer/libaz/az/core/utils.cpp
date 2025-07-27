#include "az/core/utils.hpp"

#include "az/assert.hpp"

#include <algorithm>

namespace az {

size_t elapsed_ns(const TimePoint &start_ts, const TimePoint &end_ts) {
    return std::chrono::nanoseconds(end_ts - start_ts).count();
}

auto distribute_items(size_t n_items, size_t n_workers, size_t worker_id, size_t block_size) -> Range {
    AZ_DEBUG_ASSERT(worker_id < n_workers);

    size_t n_blocks = (n_items + block_size - 1) / block_size;

    size_t n_blocks_per_worker = n_blocks / n_workers;
    size_t n_remain_blocks = n_blocks % n_workers;

    size_t offset = n_blocks_per_worker * worker_id;
    size_t count = n_blocks_per_worker;

    if (worker_id < n_remain_blocks) {
        offset += worker_id;
        count += 1;
    } else {
        offset += n_remain_blocks;
    }

    offset = offset * block_size;
    count = std::min(n_items - offset, count * block_size);

    return {
        .offset = offset,
        .count = count,
    };
}

}  // namespace az
