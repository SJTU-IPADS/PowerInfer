// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {

    CLI::App app{"App to demonstrate exclusionary option groups."};

    std::vector<int> range;
    app.add_option("--range,-R", range, "A range")->expected(-2);

    auto *ogroup = app.add_option_group("min_max_step", "set the min max and step");
    int min{0}, max{0}, step{1};
    ogroup->add_option("--min,-m", min, "The minimum")->required();
    ogroup->add_option("--max,-M", max, "The maximum")->required();
    ogroup->add_option("--step,-s", step, "The step")->capture_default_str();

    app.require_option(1);

    CLI11_PARSE(app, argc, argv);

    if(!range.empty()) {
        if(range.size() == 2) {
            min = range[0];
            max = range[1];
        }
        if(range.size() >= 3) {
            step = range[0];
            min = range[1];
            max = range[2];
        }
    }
    std::cout << "range is [" << min << ':' << step << ':' << max << "]\n";
    return 0;
}
