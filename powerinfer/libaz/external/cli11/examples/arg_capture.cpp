// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Code modified from https://github.com/CLIUtils/CLI11/issues/559

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

/** This example demonstrates the use of `prefix_command` on a subcommand
to capture all subsequent arguments along with an alias to make it appear as a regular options.

All the values after the "sub" or "--sub" are available in the remaining() method.
*/
int main(int argc, const char *argv[]) {

    int value{0};
    CLI::App app{"Test App"};
    app.add_option("-v", value, "value");

    auto *subcom = app.add_subcommand("sub", "")->prefix_command();
    subcom->alias("--sub");
    CLI11_PARSE(app, argc, argv);

    std::cout << "value=" << value << '\n';
    std::cout << "after Args:";
    for(const auto &aarg : subcom->remaining()) {
        std::cout << aarg << " ";
    }
    std::cout << '\n';
}
