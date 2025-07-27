// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifdef CLI11_SINGLE_FILE
#include "CLI11.hpp"
#else
#include "CLI/CLI.hpp"
#endif

#include "catch.hpp"
#include <string>
#include <vector>

using input_t = std::vector<std::string>;

TEST_CASE("Basic: Empty", "[simple]") {

    {
        CLI::App app;
        input_t simpleput;
        app.parse(simpleput);
    }
    {
        CLI::App app;
        input_t spare = {"spare"};
        CHECK_THROWS_AS(app.parse(spare), CLI::ExtrasError);
    }
    {
        CLI::App app;
        input_t simpleput;
        app.parse(simpleput);
    }
}
