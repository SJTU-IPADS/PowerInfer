// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {

    CLI::App app("test for positional validation");

    int num1{-1}, num2{-1};
    app.add_option("num1", num1, "first number")->check(CLI::Number);
    app.add_option("num2", num2, "second number")->check(CLI::Number);
    std::string file1, file2;
    app.add_option("file1", file1, "first file")->required();
    app.add_option("file2", file2, "second file");
    app.validate_positionals();

    CLI11_PARSE(app, argc, argv);

    if(num1 != -1)
        std::cout << "Num1 = " << num1 << '\n';

    if(num2 != -1)
        std::cout << "Num2 = " << num2 << '\n';

    std::cout << "File 1 = " << file1 << '\n';
    if(!file2.empty()) {
        std::cout << "File 2 = " << file2 << '\n';
    }

    return 0;
}
