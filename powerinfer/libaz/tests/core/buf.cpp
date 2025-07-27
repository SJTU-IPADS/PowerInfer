#include "az/core/buf.hpp"

#include <gtest/gtest.h>

TEST(Core, AlignedArray) {
    az::AlignedArray<int> arr(32);
    void *data_copy = arr.buf.data;
    size_t size_copy = arr.buf.size;

    int i = 0;
    for (int &v : arr.span()) {
        v = i + 1;
        i++;
    }

    EXPECT_EQ(arr.chunk(4, 2).ptr<int>()[0], 9);
    EXPECT_EQ(arr[23], 24);

    az::AlignedArray<int> arr2;
    arr2 = std::move(arr);
    EXPECT_EQ(arr.buf.data, nullptr);
    EXPECT_EQ(arr.buf.size, 0);

    arr.~AlignedArray<int>();
    EXPECT_EQ(arr2.buf.data, data_copy);
    EXPECT_EQ(arr2.buf.size, size_copy);

    i = 0;
    for (int v : arr.const_span()) {
        EXPECT_EQ(v, i + 1);
        i++;
    }

    az::Buf *buf_ptr = &arr2.buf;
    buf_ptr->~Buf();
    EXPECT_EQ(arr.buf.data, nullptr);
    EXPECT_EQ(arr.buf.size, 0);
}
