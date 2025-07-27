#include "az/core/aligned_alloc.hpp"

#include <gtest/gtest.h>

TEST(Core, AlignedAlloc) {
    size_t alignments[] = {16, 64, 512, 4096, 16384};

    for (size_t alignment : alignments) {
        void *ptr = az::aligned_alloc(alignment, 17);
        EXPECT_EQ(reinterpret_cast<uint64_t>(ptr) % alignment, 0) << "alignment=" << alignment;
        az::aligned_free(ptr);
    }
}
