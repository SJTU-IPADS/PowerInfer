// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../fuzz/fuzzApp.hpp"
#include "app_helper.hpp"
#include <string>
#include <vector>

std::string loadFailureFile(const std::string &type, int index) {
    std::string fileName(TEST_FILE_FOLDER "/fuzzFail/");
    fileName.append(type);
    fileName += std::to_string(index);
    std::ifstream crashFile(fileName, std::ios::in | std::ios::binary);
    if(crashFile) {
        std::vector<char> buffer(std::istreambuf_iterator<char>(crashFile), {});

        std::string cdata(buffer.begin(), buffer.end());
        return cdata;
    }
    return std::string{};
}

TEST_CASE("app_fail") {
    CLI::FuzzApp fuzzdata;
    auto app = fuzzdata.generateApp();

    int index = GENERATE(range(1, 4));
    std::string optionString;
    auto parseData = loadFailureFile("fuzz_app_fail", index);
    if(index >= 3 && parseData.size() > 25) {
        optionString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }

    try {

        if(!optionString.empty()) {
            app->add_option(optionString, fuzzdata.buffer);
        }
        try {
            app->parse(parseData);
        } catch(const CLI::ParseError & /*e*/) {
            CHECK(true);
        }
    } catch(const CLI::ConstructionError & /*e*/) {
        CHECK(true);
    }
}

TEST_CASE("file_fail") {
    CLI::FuzzApp fuzzdata;
    auto app = fuzzdata.generateApp();

    int index = GENERATE(range(1, 9));
    auto parseData = loadFailureFile("fuzz_file_fail", index);
    std::stringstream out(parseData);
    try {
        app->parse_from_stream(out);
    } catch(const CLI::ParseError & /*e*/) {
        CHECK(true);
    }
}

TEST_CASE("app_file_gen_fail") {
    CLI::FuzzApp fuzzdata;
    auto app = fuzzdata.generateApp();

    int index = GENERATE(range(1, 41));
    std::string optionString, flagString;
    auto parseData = loadFailureFile("fuzz_app_file_fail", index);
    if(parseData.size() > 25) {
        optionString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    if(parseData.size() > 25) {
        flagString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    try {

        if(!optionString.empty()) {
            app->add_option(optionString, fuzzdata.buffer);
        }
        if(!flagString.empty()) {
            app->add_flag(flagString, fuzzdata.intbuffer);
        }
        try {
            app->parse(parseData);
        } catch(const CLI::ParseError & /*e*/) {
            return;
        }
    } catch(const CLI::ConstructionError & /*e*/) {
        return;
    }
    std::string configOut = app->config_to_str();
    app->clear();
    std::stringstream out(configOut);
    app->parse_from_stream(out);
}

// this test uses the same tests as above just with a full roundtrip test
TEST_CASE("app_file_roundtrip") {
    CLI::FuzzApp fuzzdata;
    CLI::FuzzApp fuzzdata2;
    auto app = fuzzdata.generateApp();
    auto app2 = fuzzdata2.generateApp();
    int index = GENERATE(range(1, 41));
    std::string optionString, flagString;
    auto parseData = loadFailureFile("fuzz_app_file_fail", index);
    if(parseData.size() > 25) {
        optionString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    if(parseData.size() > 25) {
        flagString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    try {

        if(!optionString.empty()) {
            app->add_option(optionString, fuzzdata.buffer);
            app2->add_option(optionString, fuzzdata2.buffer);
        }
        if(!flagString.empty()) {
            app->add_flag(flagString, fuzzdata.intbuffer);
            app2->add_flag(flagString, fuzzdata2.intbuffer);
        }
        try {
            app->parse(parseData);
        } catch(const CLI::ParseError & /*e*/) {
            return;
        }
    } catch(const CLI::ConstructionError & /*e*/) {
        return;
    }
    std::string configOut = app->config_to_str();
    std::stringstream out(configOut);
    app2->parse_from_stream(out);
    bool result = fuzzdata2.compare(fuzzdata);
    /*
    if (!result)
    {
        configOut = app->config_to_str();
        result = fuzzdata2.compare(fuzzdata);
    }
    */
    CHECK(result);
}

// this test uses the same tests as above just with a full roundtrip test
TEST_CASE("app_roundtrip") {
    CLI::FuzzApp fuzzdata;
    CLI::FuzzApp fuzzdata2;
    auto app = fuzzdata.generateApp();
    auto app2 = fuzzdata2.generateApp();
    int index = GENERATE(range(1, 5));
    std::string optionString, flagString;
    auto parseData = loadFailureFile("round_trip_fail", index);
    if(parseData.size() > 25) {
        optionString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    if(parseData.size() > 25) {
        flagString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    try {

        if(!optionString.empty()) {
            app->add_option(optionString, fuzzdata.buffer);
            app2->add_option(optionString, fuzzdata2.buffer);
        }
        if(!flagString.empty()) {
            app->add_flag(flagString, fuzzdata.intbuffer);
            app2->add_flag(flagString, fuzzdata2.intbuffer);
        }
        try {
            app->parse(parseData);
        } catch(const CLI::ParseError & /*e*/) {
            return;
        }
    } catch(const CLI::ConstructionError & /*e*/) {
        return;
    }
    std::string configOut = app->config_to_str();
    std::stringstream out(configOut);
    app2->parse_from_stream(out);
    bool result = fuzzdata2.compare(fuzzdata);
    /*
    if (!result)
    {
    configOut = app->config_to_str();
    result = fuzzdata2.compare(fuzzdata);
    }
    */
    CHECK(result);
}

// same as above but just a single test for debugging
TEST_CASE("app_roundtrip_single") {
    CLI::FuzzApp fuzzdata;
    CLI::FuzzApp fuzzdata2;
    auto app = fuzzdata.generateApp();
    auto app2 = fuzzdata2.generateApp();
    int index = 5;
    std::string optionString, flagString;
    auto parseData = loadFailureFile("round_trip_fail", index);
    if(parseData.size() > 25) {
        optionString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    if(parseData.size() > 25) {
        flagString = parseData.substr(0, 25);
        parseData.erase(0, 25);
    }
    try {

        if(!optionString.empty()) {
            app->add_option(optionString, fuzzdata.buffer);
            app2->add_option(optionString, fuzzdata2.buffer);
        }
        if(!flagString.empty()) {
            app->add_flag(flagString, fuzzdata.intbuffer);
            app2->add_flag(flagString, fuzzdata2.intbuffer);
        }
        try {
            app->parse(parseData);
        } catch(const CLI::ParseError & /*e*/) {
            return;
        }
    } catch(const CLI::ConstructionError & /*e*/) {
        return;
    }
    std::string configOut = app->config_to_str();
    std::stringstream out(configOut);
    app2->parse_from_stream(out);
    bool result = fuzzdata2.compare(fuzzdata);
    /*
    if (!result)
    {
    configOut = app->config_to_str();
    result = fuzzdata2.compare(fuzzdata);
    }
    */
    CHECK(result);
}

TEST_CASE("fuzz_config_test1") {
    CLI::FuzzApp fuzzdata;
    auto app = fuzzdata.generateApp();

    std::string config_string = "<option>--new_option</option><flag>--new_flag</flag><vector>--new_vector</vector>";
    auto loc = fuzzdata.add_custom_options(app.get(), config_string);
    config_string = config_string.substr(loc);
    CHECK(config_string.empty());
    CHECK(app->get_option_no_throw("--new_option") != nullptr);
    CHECK(app->get_option_no_throw("--new_flag") != nullptr);
    CHECK(app->get_option_no_throw("--new_vector") != nullptr);
}

// this test uses the same tests as above just with a full roundtrip test
TEST_CASE("app_roundtrip_custom") {
    CLI::FuzzApp fuzzdata;
    CLI::FuzzApp fuzzdata2;
    auto app = fuzzdata.generateApp();
    auto app2 = fuzzdata2.generateApp();
    int index = GENERATE(range(1, 4));
    std::string optionString, flagString;
    auto parseData = loadFailureFile("round_trip_custom", index);
    std::size_t pstring_start{0};
    pstring_start = fuzzdata.add_custom_options(app.get(), parseData);

    if(pstring_start > 0) {
        app->parse(parseData.substr(pstring_start));
    } else {
        app->parse(parseData);
    }

    // should be able to write the config to a file and read from it again
    std::string configOut = app->config_to_str();
    app->clear();
    std::stringstream out(configOut);
    if(pstring_start > 0) {
        fuzzdata2.add_custom_options(app2.get(), parseData);
    }
    app2->parse_from_stream(out);
    auto result = fuzzdata2.compare(fuzzdata);
    CHECK(result);
}
