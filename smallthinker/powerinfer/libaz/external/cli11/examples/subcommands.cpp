// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {

    CLI::App app("K3Pi goofit fitter");
    app.set_help_all_flag("--help-all", "Expand all help");
    app.add_flag("--random", "Some random flag");
    CLI::App *start = app.add_subcommand("start", "A great subcommand");
    CLI::App *stop = app.add_subcommand("stop", "Do you really want to stop?");
    app.require_subcommand();  // 1 or more

    std::string file;
    start->add_option("-f,--file", file, "File name");

    CLI::Option *s = stop->add_flag("-c,--count", "Counter");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Working on --file from start: " << file << '\n';
    std::cout << "Working on --count from stop: " << s->count() << ", direct count: " << stop->count("--count") << '\n';
    std::cout << "Count of --random flag: " << app.count("--random") << '\n';
    for(auto *subcom : app.get_subcommands())
        std::cout << "Subcommand: " << subcom->get_name() << '\n';

    return 0;
}
