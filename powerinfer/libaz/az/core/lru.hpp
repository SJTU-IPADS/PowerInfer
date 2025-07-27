#pragma once

#include "az/core/list.hpp"

namespace az {

struct LRU {
    size_t size = 0;
    ListNode head;

    void add(ListNode &node, bool at_head = true);
    void promote(ListNode &node);
    auto evict() -> ListNode &;
};

}  // namespace az
