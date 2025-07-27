#pragma once

#include <cstddef>
#include <vector>

namespace az {

struct CPUInfo {
    size_t id;
    size_t max_freq;

    bool operator<(const CPUInfo &other) const {
        return max_freq > other.max_freq || (max_freq == other.max_freq && id > other.id);
    }
};

auto get_sorted_cpu_info_list() -> std::vector<CPUInfo>;

/**
 * Automatically choose CPU for a thread based on CPU frequencies.
 * Set at most once for each thread.
 * NOTE: Hyperthreading is not considered.
 */
void auto_set_cpu_affinity(size_t thread_id);

}  // namespace az
