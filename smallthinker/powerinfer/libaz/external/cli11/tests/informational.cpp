// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifdef CLI11_SINGLE_FILE
#include "CLI11.hpp"
#else
#include "CLI/CLI.hpp"
#endif

#include <iostream>

int main() {
    std::cout << "\nCLI11 information:\n";

    std::cout << "  C++ standard: ";
#if defined(CLI11_CPP20)
    std::cout << 20;
#elif defined(CLI11_CPP17)
    std::cout << 17;
#elif defined(CLI11_CPP14)
    std::cout << 14;
#else
    std::cout << 11;
#endif
    std::cout << "\n";

    std::cout << "  __has_include: ";
#ifdef __has_include
    std::cout << "yes\n";
#else
    std::cout << "no\n";
#endif

#if CLI11_OPTIONAL
    std::cout << "  [Available as CLI::optional]";
#else
    std::cout << "  No optional library found\n";
#endif

#if CLI11_STD_OPTIONAL
    std::cout << "  std::optional support active\n";
#endif

#if CLI11_EXPERIMENTAL_OPTIONAL
    std::cout << "  std::experimental::optional support active\n";
#endif

#if CLI11_BOOST_OPTIONAL
    std::cout << "  boost::optional support active\n";
#endif

    std::cout << '\n';
}
