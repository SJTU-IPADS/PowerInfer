// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <utility>
#include <vector>

// This example shows the usage of the retired and deprecated option helper methods
int main(int argc, char **argv) {

    CLI::App app("example for retired/deprecated options");
    std::vector<int> x;
    auto *opt1 = app.add_option("--retired_option2", x);

    std::pair<int, int> y;
    auto *opt2 = app.add_option("--deprecate", y);

    app.add_option("--not_deprecated", x);

    // specify that a non-existing option is retired
    CLI::retire_option(app, "--retired_option");

    // specify that an existing option is retired and non-functional: this will replace the option with another that
    // behaves the same but does nothing
    CLI::retire_option(app, opt1);

    // deprecate an existing option and specify the recommended replacement
    CLI::deprecate_option(opt2, "--not_deprecated");

    CLI11_PARSE(app, argc, argv);

    if(!x.empty()) {
        std::cout << "Retired option example: got --not_deprecated values:";
        for(auto &xval : x) {
            std::cout << xval << " ";
        }
        std::cout << '\n';
    } else if(app.count_all() == 1) {
        std::cout << "Retired option example: no arguments received\n";
    }
    return 0;
}
