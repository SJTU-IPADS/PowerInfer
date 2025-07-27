#include "az/core/lru.hpp"

#include <gtest/gtest.h>

TEST(Core, LRU) {
    az::LRU lru;
    az::ListNode n1, n2, n3;

    lru.add(n1);
    lru.add(n2);
    lru.add(n3, false);
    ASSERT_EQ(lru.head.next, &n2);

    lru.promote(n3);
    ASSERT_EQ(lru.head.next, &n3);

    lru.promote(n1);
    ASSERT_EQ(lru.head.next, &n1);
    ASSERT_EQ(lru.size, 3);

    ASSERT_EQ(&lru.evict(), &n2);
    ASSERT_EQ(&lru.evict(), &n3);
    ASSERT_EQ(&lru.evict(), &n1);
    ASSERT_TRUE(lru.head.empty());
    ASSERT_EQ(lru.size, 0);
}
