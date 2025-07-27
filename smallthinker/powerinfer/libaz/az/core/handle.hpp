#pragma once

#include "az/common.hpp"

#include <memory>

namespace az {

using Handle = int32_t;

static constexpr size_t max_handle_id = 65535;
static constexpr Handle invalid_handle = 0;

extern std::vector<void *> handle_table;

Handle register_handle(void *ptr);

ALWAYS_INLINE static inline void *query_handle(Handle handle) {
    return handle_table[handle];
}

template <typename T>
struct ObjectPtr {
    Arc<T> ptr;
    Handle handle = invalid_handle;

    bool operator==(const ObjectPtr &other) const {
        return ptr == other.ptr;
    }

    bool operator!=(const ObjectPtr &other) const {
        return ptr != other.ptr;
    }

    explicit operator bool() const {
        return static_cast<bool>(ptr);
    }

    T &operator*() const {
        return *ptr;
    }

    T *operator->() const {
        return ptr.get();
    }

    T *get() {
        return ptr.get();
    }
};

template <typename T, typename ... Args>
auto make_object_ptr(Args && ... args) {
    ObjectPtr<T> object;
    object.ptr = std::make_shared<T>(std::forward<Args>(args)...);
    object.handle = register_handle(object.ptr.get());
    return object;
}

}  // namespace az
