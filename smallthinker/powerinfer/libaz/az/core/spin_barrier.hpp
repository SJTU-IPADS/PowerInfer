#pragma once

#include <atomic>
#include <cstddef>

namespace az {

struct SpinBarrier {
    void init(size_t new_width);
    void wait();

private:
    size_t width = 0;
    std::atomic<size_t> count;
};

extern SpinBarrier global_spin_barrier;

}  // namespace az
