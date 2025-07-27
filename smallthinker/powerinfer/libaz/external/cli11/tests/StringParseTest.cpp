// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <cstdio>
#include <sstream>
#include <string>

TEST_CASE_METHOD(TApp, "ExistingExeCheck", "[stringparse]") {

    TempFile tmpexe{"existingExe.out"};

    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);

    {
        std::ofstream out{tmpexe};
        out << "useless string doesn't matter" << '\n';
    }

    app.parse(std::string("./") + std::string(tmpexe) +
                  R"( --string="this is my quoted string" -t 'qstring 2' -m=`"quoted string"`)",
              true);
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
}

TEST_CASE_METHOD(TApp, "ExistingExeCheckWithSpace", "[stringparse]") {

    TempFile tmpexe{"Space File.out"};

    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);

    {
        std::ofstream out{tmpexe};
        out << "useless string doesn't matter" << '\n';
    }

    app.parse(std::string("./") + std::string(tmpexe) +
                  R"( --string="this is my quoted string" -t 'qstring 2' -m=`"quoted string"`)",
              true);
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);

    CHECK(std::string("./") + std::string(tmpexe) == app.get_name());
}

TEST_CASE_METHOD(TApp, "ExistingExeCheckWithLotsOfSpace", "[stringparse]") {

    TempFile tmpexe{"this is a weird file.exe"};

    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);

    {
        std::ofstream out{tmpexe};
        out << "useless string doesn't matter" << '\n';
    }

    app.parse(std::string("./") + std::string(tmpexe) +
                  R"( --string="this is my quoted string" -t 'qstring 2' -m=`"quoted string"`)",
              true);
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);

    CHECK(std::string("./") + std::string(tmpexe) == app.get_name());
}

// From GitHub issue #591 https://github.com/CLIUtils/CLI11/issues/591
TEST_CASE_METHOD(TApp, "ProgNameWithSpace", "[stringparse]") {

    app.add_flag("--foo");
    CHECK_NOTHROW(app.parse("\"Foo Bar\" --foo", true));

    CHECK(app["--foo"]->as<bool>());
    CHECK(app.get_name() == "Foo Bar");
}

// From GitHub issue #739 https://github.com/CLIUtils/CLI11/issues/739
TEST_CASE_METHOD(TApp, "ProgNameOnly", "[stringparse]") {

    app.add_flag("--foo");
    CHECK_NOTHROW(app.parse("\"C:\\example.exe\"", true));

    CHECK(app.get_name() == "C:\\example.exe");
}

TEST_CASE_METHOD(TApp, "ProgNameWithSpaceEmbeddedQuote", "[stringparse]") {

    app.add_flag("--foo");
    CHECK_NOTHROW(app.parse("\"Foo\\\" Bar\" --foo", true));

    CHECK(app["--foo"]->as<bool>());
    CHECK(app.get_name() == "Foo\" Bar");
}

TEST_CASE_METHOD(TApp, "ProgNameWithSpaceSingleQuote", "[stringparse]") {

    app.add_flag("--foo");
    CHECK_NOTHROW(app.parse(R"('Foo\' Bar' --foo)", true));

    CHECK(app["--foo"]->as<bool>());
    CHECK(app.get_name() == "Foo' Bar");
}
