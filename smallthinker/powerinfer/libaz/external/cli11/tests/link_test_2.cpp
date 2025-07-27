// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "CLI/CLI.hpp"
#include "CLI/Timer.hpp"
#include "catch.hpp"

int do_nothing();

// Verifies there are no unguarded inlines
TEST_CASE("Link: DoNothing", "[link]") {
    int a = do_nothing();
    CHECK(a == 7);
}
