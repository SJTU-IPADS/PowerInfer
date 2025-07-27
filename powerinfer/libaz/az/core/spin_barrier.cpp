#include "az/core/spin_barrier.hpp"

#include "az/assert.hpp"
#include "az/core/cpu_yield.hpp"

namespace az {

void SpinBarrier::init(size_t new_width) {
    width = new_width;
    count = 0;
}

void SpinBarrier::wait() {
    AZ_DEBUG_ASSERT(width > 0);

    size_t current = count.fetch_add(1, std::memory_order_acq_rel);
    size_t target = (current / width + 1) * width;
    while (count.load(std::memory_order_relaxed) < target) {
        cpu_yield();
    }
}

SpinBarrier global_spin_barrier;

}  // namespace az
