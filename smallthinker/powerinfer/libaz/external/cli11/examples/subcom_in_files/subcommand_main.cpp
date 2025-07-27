// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "subcommand_a.hpp"
#include <CLI/CLI.hpp>

int main(int argc, char **argv) {
    CLI::App app{"..."};

    // Call the setup functions for the subcommands.
    // They are kept alive by a shared pointer in the
    // lambda function held by CLI11
    setup_subcommand_a(app);

    // Make sure we get at least one subcommand
    app.require_subcommand();

    // More setup if needed, i.e., other subcommands etc.

    CLI11_PARSE(app, argc, argv);

    return 0;
}
