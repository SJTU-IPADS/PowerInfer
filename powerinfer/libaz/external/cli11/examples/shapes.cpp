// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {

    CLI::App app("load shapes");

    app.set_help_all_flag("--help-all");
    auto *circle = app.add_subcommand("circle", "draw a circle")->immediate_callback();
    double radius{0.0};
    int circle_counter{0};
    circle->callback([&radius, &circle_counter] {
        ++circle_counter;
        std::cout << "circle" << circle_counter << " with radius " << radius << '\n';
    });

    circle->add_option("radius", radius, "the radius of the circle")->required();

    auto *rect = app.add_subcommand("rectangle", "draw a rectangle")->immediate_callback();
    double edge1{0.0};
    double edge2{0.0};
    int rect_counter{0};
    rect->callback([&edge1, &edge2, &rect_counter] {
        ++rect_counter;
        if(edge2 == 0) {
            edge2 = edge1;
        }
        std::cout << "rectangle" << rect_counter << " with edges [" << edge1 << ',' << edge2 << "]\n";
        edge2 = 0;
    });

    rect->add_option("edge1", edge1, "the first edge length of the rectangle")->required();
    rect->add_option("edge2", edge2, "the second edge length of the rectangle");

    auto *tri = app.add_subcommand("triangle", "draw a rectangle")->immediate_callback();
    std::vector<double> sides;
    int tri_counter = 0;
    tri->callback([&sides, &tri_counter] {
        ++tri_counter;

        std::cout << "triangle" << tri_counter << " with sides [" << CLI::detail::join(sides) << "]\n";
    });

    tri->add_option("sides", sides, "the side lengths of the triangle");

    CLI11_PARSE(app, argc, argv);

    return 0;
}
