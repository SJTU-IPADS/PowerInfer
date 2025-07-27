#pragma once

#include <atomic>

namespace az {

class SpinLock {
public:
    bool try_lock() {
        return !flag.test_and_set(std::memory_order_acquire);
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

}  // namespace az
