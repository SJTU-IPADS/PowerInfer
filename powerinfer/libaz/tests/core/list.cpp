#include "az/core/list.hpp"

#include <gtest/gtest.h>

TEST(Core, ListLinkDetach) {
    az::ListNode n[4];
    for (size_t i = 0; i + 1 < 4; i++) {
        n[i].link_to(n[i + 1]);
    }

    for (size_t i = 0; i < 4; i++) {
        EXPECT_EQ(n[i].next, &n[(i + 1) % 4]);
        EXPECT_EQ(n[(i + 1) % 4].prev, &n[i]);
    }

    n[1].detach();
    EXPECT_EQ(n[0].next, &n[2]);
    EXPECT_EQ(n[2].prev, &n[0]);
    EXPECT_TRUE(n[1].empty());
    EXPECT_EQ(n[1].next, &n[1]);
    EXPECT_EQ(n[1].prev, &n[1]);
}

TEST(Core, ListMoveCtor) {
    az::ListNode node1;
    EXPECT_EQ(node1.next, &node1);
    EXPECT_EQ(node1.prev, &node1);

    az::ListNode node2 = std::move(node1);
    EXPECT_EQ(node2.next, &node2);
    EXPECT_EQ(node2.prev, &node2);
    EXPECT_EQ(node1.next, nullptr);
    EXPECT_EQ(node1.prev, nullptr);
}

TEST(Core, ListMoveCopy) {
    az::ListNode n1, n2, n3;
    n1.link_to(n2);
    n2.link_to(n3);

    az::ListNode n4;
    n4 = std::move(n2);

    EXPECT_EQ(n2.next, nullptr);
    EXPECT_EQ(n2.prev, nullptr);
    EXPECT_EQ(n1.next, &n4);
    EXPECT_EQ(n4.next, &n3);
    EXPECT_EQ(n3.next, &n1);
    EXPECT_EQ(n1.prev, &n3);
    EXPECT_EQ(n3.prev, &n4);
    EXPECT_EQ(n4.prev, &n1);
}

TEST(Core, ListDtor) {
    az::ListNode n1, n2, n3;
    n1.link_to(n2);
    n2.link_to(n3);
    n2.~ListNode();

    EXPECT_EQ(n2.next, nullptr);
    EXPECT_EQ(n2.prev, nullptr);
    EXPECT_EQ(n1.next, nullptr);
    EXPECT_EQ(n3.prev, nullptr);
}

TEST(Core, ListEntry) {
    struct Owner {
        int payload;
        az::ListNode node;

        Owner(int payload) : payload(payload) {}
    };

    Owner o(233);
    ASSERT_EQ(o.node.get_owner_ptr(&Owner::node), &o);

    struct NestedOwner {
        int a;

        struct {
            int c;
            az::ListNode node;
        } b;
    };

    NestedOwner no;
    EXPECT_EQ(az_list_entry(&no.b.node, NestedOwner, b.node), &no);
}

TEST(Core, ListMove) {
    az::ListNode n1, n2, n3;
    n1.link_to(n2);
    n1.move_to(n3);
    EXPECT_TRUE(n2.empty());
    EXPECT_EQ(n1.next, &n3);
    EXPECT_EQ(n1.prev, &n3);
}
