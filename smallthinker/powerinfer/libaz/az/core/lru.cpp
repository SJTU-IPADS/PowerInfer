#include "az/core/lru.hpp"

#include "az/assert.hpp"

namespace az {

void LRU::add(az::ListNode &node, bool at_head) {
    AZ_DEBUG_ASSERT(node.empty());

    size++;

    if (at_head) {
        head.link_to(node);
    } else {
        node.link_to(head);
    }
}

void LRU::promote(az::ListNode &node) {
    AZ_DEBUG_ASSERT(!node.empty());

    node.detach();
    head.link_to(node);
}

auto LRU::evict() -> az::ListNode & {
    AZ_DEBUG_ASSERT(!head.empty());

    size--;
    auto *node = head.prev;
    node->detach();
    return *node;
}

}  // namespace az
