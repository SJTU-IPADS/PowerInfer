#include "CLI/CLI.hpp"
#include <iostream>

int main(int argc, char **argv) {
    CLI::App app{"App description"};

    // Define options
    int p = 0;
    app.add_option("-p", p, "Parameter");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Parameter value: " << p << std::endl;
    return 0;
}
