// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    CLI::App test{R"raw(Modify the help print so that argument values are accessible.
Note that this will not shortcut `->required` and other similar options.)raw"};

    // Remove help flag because it shortcuts all processing
    test.set_help_flag();

    // Add custom flag that activates help
    auto *help = test.add_flag("-h,--help", "Request help");

    std::string some_option;
    test.add_option("-a", some_option, "Some description");

    try {
        test.parse(argc, argv);
        if(*help)
            throw CLI::CallForHelp();
    } catch(const CLI::Error &e) {
        std::cout << "Option -a string in help: " << some_option << '\n';
        return test.exit(e);
    }

    std::cout << "Option -a string: " << some_option << '\n';
    return 0;
}
