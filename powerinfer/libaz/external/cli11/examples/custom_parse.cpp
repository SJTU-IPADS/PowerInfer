// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// from Issue #566 on github https://github.com/CLIUtils/CLI11/issues/566

#include <CLI/CLI.hpp>
#include <iostream>
#include <sstream>
#include <string>

// example file to demonstrate a custom lexical cast function

template <class T = int> struct Values {
    T a;
    T b;
    T c;
};

// in C++20 this is constructible from a double due to the new aggregate initialization in C++20.
using DoubleValues = Values<double>;

// the lexical cast operator should be in the same namespace as the type for ADL to work properly
bool lexical_cast(const std::string &input, Values<double> & /*v*/) {
    std::cout << "called correct lexical_cast function ! val: " << input << '\n';
    return true;
}

DoubleValues doubles;
void argparse(CLI::Option_group *group) { group->add_option("--dv", doubles)->default_str("0"); }

int main(int argc, char **argv) {
    CLI::App app;

    argparse(app.add_option_group("param"));
    CLI11_PARSE(app, argc, argv);
    return 0;
}
