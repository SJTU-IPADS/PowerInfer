#include "az/assert.hpp"

int main() {
    AZ_ASSERT(false);
    AZ_ASSERT_EQ(1, 0);

    AZ_DEBUG_ASSERT(true) << [] { fmt::println("Not printed"); };
    AZ_DEBUG_ASSERT(233 == 244) << [] { fmt::println("Printed"); };

    return 0;
}
