#include "az/init.hpp"

#include <gtest/gtest.h>

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    az::init();
    int ret = RUN_ALL_TESTS();
    az::deinit();

    return ret;
}
