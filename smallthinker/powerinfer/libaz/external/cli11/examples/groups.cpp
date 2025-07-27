// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <CLI/Timer.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    CLI::AutoTimer give_me_a_name("This is a timer");

    CLI::App app("K3Pi goofit fitter");

    std::string file;
    CLI::Option *opt = app.add_option("-f,--file,file", file, "File name")->required()->group("Important");

    int count{0};
    CLI::Option *copt = app.add_flag("-c,--count", count, "Counter")->required()->group("Important");

    double value{0.0};  // = 3.14;
    app.add_option("-d,--double", value, "Some Value")->group("Other");

    try {
        app.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        return app.exit(e);
    }

    std::cout << "Working on file: " << file << ", direct count: " << app.count("--file")
              << ", opt count: " << opt->count() << '\n';
    std::cout << "Working on count: " << count << ", direct count: " << app.count("--count")
              << ", opt count: " << copt->count() << '\n';
    std::cout << "Some value: " << value << '\n';

    return 0;
}
