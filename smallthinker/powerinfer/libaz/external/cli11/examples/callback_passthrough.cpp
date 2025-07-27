// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {

    CLI::App app("callback_passthrough");
    app.allow_extras();
    std::string argName;
    std::string val;
    app.add_option("--argname", argName, "the name of the custom command line argument");
    app.callback([&app, &val, &argName]() {
        if(!argName.empty()) {
            CLI::App subApp;
            subApp.add_option("--" + argName, val, "custom argument option");
            subApp.parse(app.remaining_for_passthrough());
        }
    });

    CLI11_PARSE(app, argc, argv);
    std::cout << "the value is now " << val << '\n';
}
