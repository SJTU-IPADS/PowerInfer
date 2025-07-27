#pragma once

#include <cstddef>
#include <algorithm>

struct ItemBatch {
    size_t offset;
    size_t count;

    auto begin() const -> size_t {
        return offset;
    }

    auto end() const -> size_t {
        return offset + count;
    }
};

// Distribute items evenly among workers, at the granularity of block_size
static inline auto distribute_items(size_t n_items, size_t n_workers, size_t worker_id, size_t block_size) -> ItemBatch {
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
