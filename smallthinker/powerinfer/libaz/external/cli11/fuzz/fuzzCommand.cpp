// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "fuzzApp.hpp"
#include <CLI/CLI.hpp>
#include <iostream>

int main(int argc, char **argv) {

    CLI::FuzzApp fuzzdata;

    auto app = fuzzdata.generateApp();
    try {

        app->parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        (app)->exit(e);
        // this just indicates we caught an error known by CLI
    }

    return 0;
}
