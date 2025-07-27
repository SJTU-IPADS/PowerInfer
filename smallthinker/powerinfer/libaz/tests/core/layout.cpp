#include "az/core/layout.hpp"

#include <gtest/gtest.h>

TEST(Core, Layout2) {
    az::Layout<2> layout(1230, 456);
    EXPECT_EQ(layout.index(1), 1 * 456);
    EXPECT_EQ(layout.index(376, 123), 376 * 456 + 123);
}

TEST(Core, Layout4) {
    az::Layout<4> layout(32, 16, 8, 24);
    EXPECT_EQ(layout.index(), 0);
    EXPECT_EQ(layout.index(1, 0, 1), 1 * 16 * 8 * 24 + 1 * 24);
    EXPECT_EQ(layout.index(1, 2, 3, 4), 1 * 16 * 8 * 24 + 2 * 8 * 24 + 3 * 24 + 4);
}

TEST(Core, Layout4Partial) {
    az::Layout<4> layout(111, 222);
    EXPECT_EQ(layout.index(1, 1, 0, 0), 1 * 222 + 1);
}
