// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

int main(int argc, char **argv) {
    CLI::App app{"An app to practice mixing unlimited arguments, but still recover the original order."};

    std::vector<int> foos;
    auto *foo = app.add_option("--foo,-f", foos, "Some unlimited argument");

    std::vector<int> bars;
    auto *bar = app.add_option("--bar", bars, "Some unlimited argument");

    app.add_flag("--z,--x", "Random other flags");

    // Standard parsing lines (copy and paste in, or use CLI11_PARSE)
    try {
        app.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        return app.exit(e);
    }

    // I prefer using the back and popping
    std::reverse(std::begin(foos), std::end(foos));
    std::reverse(std::begin(bars), std::end(bars));

    std::vector<std::pair<std::string, int>> keyval;
    for(auto *option : app.parse_order()) {
        if(option == foo) {
            keyval.emplace_back("foo", foos.back());
            foos.pop_back();
        }
        if(option == bar) {
            keyval.emplace_back("bar", bars.back());
            bars.pop_back();
        }
    }

    // Prove the vector is correct
    for(auto &pair : keyval) {
        std::cout << pair.first << " : " << pair.second << '\n';
    }
}
