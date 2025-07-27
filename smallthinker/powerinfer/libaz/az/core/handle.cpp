#include "az/core/handle.hpp"

#include "az/assert.hpp"

#include <atomic>
#include <mutex>

namespace az {

std::vector<void *> handle_table;

Handle register_handle(void *ptr) {
    static std::once_flag flag;
    static std::atomic<Handle> next_handle_id{1};

    std::call_once(flag, [&] { handle_table.resize(max_handle_id + 1); });

    auto handle_id = next_handle_id.fetch_add(1, std::memory_order_relaxed);
    AZ_ASSERT((size_t)handle_id <= max_handle_id);

    handle_table[handle_id] = ptr;

    return handle_id;
}

}  // namespace az
