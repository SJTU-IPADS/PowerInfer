// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "CLI/Timer.hpp"

#include "catch.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

TEST_CASE("Timer: MSTimes", "[timer]") {
    CLI::Timer timer{"My Timer"};
    std::this_thread::sleep_for(std::chrono::milliseconds(123));
    std::string output = timer.to_string();
    std::string new_output = (timer / 1000000).to_string();
    CHECK_THAT(output, Contains("My Timer"));
    CHECK_THAT(output, Contains(" ms"));
    CHECK_THAT(new_output, Contains(" ns"));
}

/* Takes too long
TEST_CASE("Timer: STimes", "[timer]") {
    CLI::Timer timer;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string output = timer.to_string();
    CHECK_THAT (output, Contains(" s"));
}
*/

// Fails on Windows
// TEST_CASE("Timer: UStimes", "[timer]") {
//    CLI::Timer timer;
//    std::this_thread::sleep_for(std::chrono::microseconds(2));
//    std::string output = timer.to_string();
//    CHECK_THAT (output, Contains(" ms"));
//}

TEST_CASE("Timer: BigTimer", "[timer]") {
    CLI::Timer timer{"My Timer", CLI::Timer::Big};
    std::string output = timer.to_string();
    CHECK_THAT(output, Contains("Time ="));
    CHECK_THAT(output, Contains("-----------"));
}

TEST_CASE("Timer: AutoTimer", "[timer]") {
    CLI::AutoTimer timer;
    std::string output = timer.to_string();
    CHECK_THAT(output, Contains("Timer"));
}

TEST_CASE("Timer: PrintTimer", "[timer]") {
    std::stringstream out;
    CLI::AutoTimer timer;
    out << timer;
    std::string output = out.str();
    CHECK_THAT(output, Contains("Timer"));
}

TEST_CASE("Timer: TimeItTimer", "[timer]") {
    CLI::Timer timer;
    std::string output = timer.time_it([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }, .1);
    std::cout << output << '\n';
    CHECK_THAT(output, Contains("ms"));
}
