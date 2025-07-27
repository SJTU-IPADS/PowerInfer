// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <string>

class MyFormatter : public CLI::Formatter {
  public:
    MyFormatter() : Formatter() {}
    std::string make_option_opts(const CLI::Option *) const override { return " OPTION"; }
};

int main(int argc, char **argv) {
    CLI::App app;
    app.set_help_all_flag("--help-all", "Show all help");

    auto fmt = std::make_shared<MyFormatter>();
    fmt->column_width(15);
    app.formatter(fmt);

    app.add_flag("--flag", "This is a flag");

    auto *sub1 = app.add_subcommand("one", "Description One");
    sub1->add_flag("--oneflag", "Some flag");
    auto *sub2 = app.add_subcommand("two", "Description Two");
    sub2->add_flag("--twoflag", "Some other flag");

    CLI11_PARSE(app, argc, argv);

    std::cout << "This app was meant to show off the formatter, run with -h" << '\n';

    return 0;
}
