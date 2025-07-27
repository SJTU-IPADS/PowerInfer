#include "az/core/utils.hpp"

#include <gtest/gtest.h>

TEST(Core, DistributeItems) {
    az::Range chunk[5];

    for (size_t i = 0; i < 5; i++) {
        chunk[i] = az::distribute_items(8, 5, i);
    }

    EXPECT_EQ(chunk[0], (az::Range{0, 2}));
    EXPECT_EQ(chunk[1], (az::Range{2, 2}));
    EXPECT_EQ(chunk[2], (az::Range{4, 2}));
    EXPECT_EQ(chunk[3], (az::Range{6, 1}));
    EXPECT_EQ(chunk[4], (az::Range{7, 1}));

    for (size_t i = 0; i < 5; i++) {
        chunk[i] = az::distribute_items(17, 5, i, 2);
    }

    EXPECT_EQ(chunk[0], (az::Range{0, 4}));
    EXPECT_EQ(chunk[1], (az::Range{4, 4}));
    EXPECT_EQ(chunk[2], (az::Range{8, 4}));
    EXPECT_EQ(chunk[3], (az::Range{12, 4}));
    EXPECT_EQ(chunk[4], (az::Range{16, 1}));
}
