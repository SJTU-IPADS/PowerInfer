// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "fuzzApp.hpp"
#include <CLI/CLI.hpp>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if(Size == 0) {
        return 0;
    }
    std::string parseString(reinterpret_cast<const char *>(Data), Size);
    std::stringstream out(parseString);
    CLI::FuzzApp fuzzdata;

    auto app = fuzzdata.generateApp();
    try {
        app->parse_from_stream(out);
        // should be able to write the config to a file and read from it again
        std::string configOut = app->config_to_str();

        app->clear();
        std::stringstream out(configOut);
        app->parse_from_stream(out);

    } catch(const CLI::ParseError &e) {
        // (app)->exit(e);
        // this just indicates we caught an error known by CLI
    }

    return 0;  // Non-zero return values are reserved for future use.
}
