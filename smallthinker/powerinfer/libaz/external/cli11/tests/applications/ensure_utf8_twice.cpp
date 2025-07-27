// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <cstring>
#include <iostream>

int main(int argc, char **argv) {
    CLI::App app{"App description"};
    char **original_argv = argv;
    argv = app.ensure_utf8(argv);
    argv = app.ensure_utf8(argv);  // completely useless but works ok

#ifdef _WIN32
    for(int i = 0; i < argc; i++) {
        if(std::strcmp(argv[i], original_argv[i]) != 0) {
            std::cerr << argv[i] << "\n";
            std::cerr << original_argv[i] << "\n";
            return i + 1;
        }
        argv[i][0] = 'x';  // access it to check that it is accessible
    }

#else
    (void)argc;

    if(original_argv != argv) {
        return -1;
    }
#endif

    return 0;
}
