// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"
#include <string>

TEST_CASE_METHOD(TApp, "True Bool Option", "[bool][flag]") {
    // Strings needed here due to MSVC 2015.
    auto param = GENERATE(as<std::string>{}, "true", "on", "True", "ON");
    bool value{false};  // Not used, but set just in case
    app.add_option("-b,--bool", value);
    args = {"--bool", param};
    run();
    CHECK(app.count("--bool") == 1u);
    CHECK(value);
}

TEST_CASE_METHOD(TApp, "False Bool Option", "[bool][flag]") {
    auto param = GENERATE(as<std::string>{}, "false", "off", "False", "OFF");

    bool value{true};  // Not used, but set just in case
    app.add_option("-b,--bool", value);
    args = {"--bool", param};
    run();
    CHECK(app.count("--bool") == 1u);
    CHECK_FALSE(value);
}
