// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {

    CLI::App app("configuration print example");

    app.add_flag("-p,--print", "Print configuration and exit")->configurable(false);  // NEW: print flag

    std::string file;
    CLI::Option *opt = app.add_option("-f,--file,file", file, "File name")
                           ->capture_default_str()
                           ->run_callback_for_default();  // NEW: capture_default_str()

    int count{0};
    CLI::Option *copt =
        app.add_option("-c,--count", count, "Counter")->capture_default_str();  // NEW: capture_default_str()

    int v{0};
    CLI::Option *flag = app.add_flag("--flag", v, "Some flag that can be passed multiple times")
                            ->capture_default_str();  // NEW: capture_default_str()

    double value{0.0};                                                          // = 3.14;
    app.add_option("-d,--double", value, "Some Value")->capture_default_str();  // NEW: capture_default_str()

    app.get_config_formatter_base()->quoteCharacter('"', '"');

    CLI11_PARSE(app, argc, argv);

    if(app.get_option("--print")->as<bool>()) {  // NEW: print configuration and exit
        std::cout << app.config_to_str(true, false);
        return 0;
    }

    std::cout << "Working on file: " << file << ", direct count: " << app.count("--file")
              << ", opt count: " << opt->count() << '\n';
    std::cout << "Working on count: " << count << ", direct count: " << app.count("--count")
              << ", opt count: " << copt->count() << '\n';
    std::cout << "Received flag: " << v << " (" << flag->count() << ") times\n";
    std::cout << "Some value: " << value << '\n';

    return 0;
}
