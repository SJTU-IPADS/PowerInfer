// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {

    CLI::App app("Prefix command app");
    app.prefix_command();

    std::vector<int> vals;
    app.add_option("--vals,-v", vals)->expected(-1);

    CLI11_PARSE(app, argc, argv);

    std::vector<std::string> more_comms = app.remaining();

    std::cout << "Prefix";
    for(int v : vals)
        std::cout << ": " << v << " ";

    std::cout << '\n' << "Remaining commands: ";

    for(const auto &com : more_comms)
        std::cout << com << " ";
    std::cout << '\n';

    return 0;
}
