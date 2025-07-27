// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "fuzzApp.hpp"
#include <CLI/CLI.hpp>
#include <cstring>
#include <exception>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if(Size == 0) {
        return 0;
    }
    std::string parseString(reinterpret_cast<const char *>(Data), Size);

    CLI::FuzzApp fuzzdata;
    CLI::FuzzApp fuzzdata2;
    auto app = fuzzdata.generateApp();
    auto app2 = fuzzdata2.generateApp();
    std::size_t pstring_start{0};
    try {
        pstring_start = fuzzdata.add_custom_options(app.get(), parseString);
    } catch(const CLI::ConstructionError &e) {
        return 0;  // Non-zero return values are reserved for future use.
    }

    try {
        if(pstring_start > 0) {
            app->parse(parseString.substr(pstring_start));
        } else {
            app->parse(parseString);
        }

    } catch(const CLI::ParseError &e) {
        //(app)->exit(e);
        // this just indicates we caught an error known by CLI
        return 0;  // Non-zero return values are reserved for future use.
    }
    // should be able to write the config to a file and read from it again
    std::string configOut = app->config_to_str();
    app->clear();
    std::stringstream out(configOut);
    if(pstring_start > 0) {
        fuzzdata2.add_custom_options(app2.get(), parseString);
    }
    app2->parse_from_stream(out);
    auto result = fuzzdata2.compare(fuzzdata);
    if(!result) {
        throw CLI::ValidationError("fuzzer", "file input results don't match parse results");
    }
    return 0;
}
