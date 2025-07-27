// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"
#include <cmath>

#include <array>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

TEST_CASE_METHOD(TApp, "OneFlagShort", "[app]") {
    app.add_flag("-c,--count");
    args = {"-c"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
}

TEST_CASE_METHOD(TApp, "OneFlagShortValues", "[app]") {
    app.add_flag("-c{v1},--count{v2}");
    args = {"-c"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
    const auto &v = app["-c"]->results();
    CHECK("v1" == v[0]);

    CHECK_THROWS_AS(app["--invalid"], CLI::OptionNotFound);
}

TEST_CASE_METHOD(TApp, "OneFlagShortValuesAs", "[app]") {
    auto *flg = app.add_flag("-c{1},--count{2}");
    args = {"-c"};
    run();
    const auto *opt = app["-c"];
    CHECK(1 == opt->as<int>());
    args = {"--count"};
    run();
    CHECK(2 == opt->as<int>());
    flg->take_first();
    args = {"-c", "--count"};
    run();
    CHECK(1 == opt->as<int>());
    flg->take_last();
    CHECK(2 == opt->as<int>());
    flg->multi_option_policy(CLI::MultiOptionPolicy::Throw);
    CHECK_THROWS_AS(opt->as<int>(), CLI::ArgumentMismatch);
    flg->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);
    auto vec = opt->as<std::vector<int>>();
    CHECK(1 == vec[0]);
    CHECK(2 == vec[1]);

    flg->multi_option_policy(CLI::MultiOptionPolicy::Sum);
    vec = opt->as<std::vector<int>>();
    CHECK(3 == vec[0]);
    CHECK(vec.size() == 1);

    flg->multi_option_policy(CLI::MultiOptionPolicy::Join);
    CHECK("1\n2" == opt->as<std::string>());
    flg->delimiter(',');
    CHECK("1,2" == opt->as<std::string>());
    flg->multi_option_policy(CLI::MultiOptionPolicy::Reverse)->expected(1, 300);
    vec = opt->as<std::vector<int>>();
    REQUIRE(vec.size() == 2U);
    CHECK(2 == vec[0]);
    CHECK(1 == vec[1]);
}

TEST_CASE_METHOD(TApp, "OneFlagShortWindows", "[app]") {
    app.add_flag("-c,--count");
    args = {"/c"};
    app.allow_windows_style_options();
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
}

TEST_CASE_METHOD(TApp, "CountNonExist", "[app]") {
    app.add_flag("-c,--count");
    args = {"-c"};
    run();
    CHECK_THROWS_AS(app.count("--nonexist"), CLI::OptionNotFound);
}

TEST_CASE_METHOD(TApp, "OneFlagLong", "[app]") {
    app.add_flag("-c,--count");
    args = {"--count"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
}

TEST_CASE_METHOD(TApp, "DashedOptions", "[app]") {
    app.add_flag("-c");
    app.add_flag("--q");
    app.add_flag("--this,--that");

    args = {"-c", "--q", "--this", "--that"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--q") == 1u);
    CHECK(app.count("--this") == 2u);
    CHECK(app.count("--that") == 2u);
}

TEST_CASE_METHOD(TApp, "DashedOptionsSingleString", "[app]") {
    app.add_flag("-c");
    app.add_flag("--q");
    app.add_flag("--this,--that");

    app.parse("-c --q --this --that");
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--q") == 1u);
    CHECK(app.count("--this") == 2u);
    CHECK(app.count("--that") == 2u);
}

TEST_CASE_METHOD(TApp, "StrangeFlagNames", "[app]") {
    app.add_flag("-=");
    app.add_flag("--t\tt");
    app.add_flag("-{");
    CHECK_THROWS_AS(app.add_flag("--t t"), CLI::ConstructionError);
    args = {"-=", "--t\tt"};
    run();
    CHECK(app.count("-=") == 1u);
    CHECK(app.count("--t\tt") == 1u);
}

TEST_CASE_METHOD(TApp, "RequireOptionsError", "[app]") {
    app.add_flag("-c");
    app.add_flag("--q");
    app.add_flag("--this,--that");
    app.set_help_flag("-h,--help");
    app.set_help_all_flag("--help_all");
    app.require_option(1, 2);
    try {
        app.parse("-c --q --this --that");
    } catch(const CLI::RequiredError &re) {
        CHECK_THAT(re.what(), !Contains("-h,--help"));
        CHECK_THAT(re.what(), !Contains("help_all"));
    }

    CHECK_NOTHROW(app.parse("-c --q"));
    CHECK_NOTHROW(app.parse("-c --this --that"));
}

TEST_CASE_METHOD(TApp, "BoolFlagOverride", "[app]") {
    bool val{false};
    auto *flg = app.add_flag("--this,--that", val);

    app.parse("--this");
    CHECK(val);
    app.parse("--this=false");
    CHECK(!val);
    flg->disable_flag_override(true);
    app.parse("--this");
    CHECK(val);
    // this is allowed since the matching string is the default
    app.parse("--this=true");
    CHECK(val);

    CHECK_THROWS_AS(app.parse("--this=false"), CLI::ArgumentMismatch);
    // try a string that specifies 'use default val'
    CHECK_NOTHROW(app.parse("--this={}"));
}

TEST_CASE_METHOD(TApp, "OneFlagRef", "[app]") {
    int ref{0};
    app.add_flag("-c,--count", ref);
    args = {"--count"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
    CHECK(ref == 1);
}

TEST_CASE_METHOD(TApp, "OneFlagRefValue", "[app]") {
    int ref{0};
    app.add_flag("-c,--count", ref);
    args = {"--count=7"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
    CHECK(ref == 7);
}

TEST_CASE_METHOD(TApp, "OneFlagRefValueFalse", "[app]") {
    int ref{0};
    auto *flg = app.add_flag("-c,--count", ref);
    args = {"--count=false"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
    CHECK(ref == -1);

    CHECK(!flg->check_fname("c"));
    args = {"--count=0"};
    run();
    CHECK(app.count("-c") == 1u);
    CHECK(app.count("--count") == 1u);
    CHECK(ref == 0);

    args = {"--count=happy"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "FlagNegation", "[app]") {
    int ref{0};
    auto *flg = app.add_flag("-c,--count,--ncount{false}", ref);
    args = {"--count", "-c", "--ncount"};
    CHECK(!flg->check_fname("count"));
    CHECK(flg->check_fname("ncount"));
    run();
    CHECK(app.count("-c") == 3u);
    CHECK(app.count("--count") == 3u);
    CHECK(app.count("--ncount") == 3u);
    CHECK(ref == 1);
}

TEST_CASE_METHOD(TApp, "FlagNegationShortcutNotation", "[app]") {
    int ref{0};
    app.add_flag("-c,--count{true},!--ncount", ref);
    args = {"--count=TRUE", "-c", "--ncount"};
    run();
    CHECK(app.count("-c") == 3u);
    CHECK(app.count("--count") == 3u);
    CHECK(app.count("--ncount") == 3u);
    CHECK(ref == 1);
}

TEST_CASE_METHOD(TApp, "FlagNegationShortcutNotationInvalid", "[app]") {
    int ref{0};
    app.add_flag("-c,--count,!--ncount", ref);
    args = {"--ncount=happy"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "OneString", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    args = {"--string", "mystring"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneWideString", "[app]") {
    std::wstring str;
    app.add_option("-s,--string", str);
    args = {"--string", "mystring"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK(L"mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringWideInput", "[app][unicode]") {
    std::string str;
    app.add_option("-s,--string", str);

    std::array<const wchar_t *, 3> cmdline{{L"app", L"--string", L"mystring"}};
    app.parse(static_cast<int>(cmdline.size()), cmdline.data());

    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringWindowsStyle", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    args = {"/string", "mystring"};
    app.allow_windows_style_options();
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringSingleStringInput", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);

    app.parse("--string mystring");
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringSingleWideStringInput", "[app][unicode]") {
    std::string str;
    app.add_option("-s,--string", str);

    app.parse(L"--string mystring");
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersion", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    args = {"--string=mystring"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionWindowsStyle", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    args = {"/string:mystring"};
    app.allow_windows_style_options();
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleString", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    app.parse("--string=mystring");
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuoted", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    app.parse(R"raw(--string="this is my quoted string")raw");
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("this is my quoted string" == str);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuotedMultiple", "[app]") {
    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.parse(R"raw(--string="this is my quoted string" -t 'qstring 2' -m=`"quoted string"`)raw");
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringEmbeddedEqual", "[app]") {
    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.parse(R"raw(--string="app=\"test1 b\" test2=\"frogs\"" -t 'qstring 2' -m=`"quoted string"`)raw");
    CHECK("app=\"test1 b\" test2=\"frogs\"" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);

    app.parse(R"raw(--string="app='test1 b' test2='frogs'" -t 'qstring 2' -m=`"quoted string"`)raw");
    CHECK("app='test1 b' test2='frogs'" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringEmbeddedEqualWindowsStyle", "[app]") {
    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("--mstr", str3);
    app.allow_windows_style_options();
    app.parse(R"raw(/string:"app:\"test1 b\" test2:\"frogs\"" /t 'qstring 2' /mstr:`"quoted string"`)raw");
    CHECK("app:\"test1 b\" test2:\"frogs\"" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);

    app.parse(R"raw(/string:"app:'test1 b' test2:'frogs'" /t 'qstring 2' /mstr:`"quoted string"`)raw");
    CHECK("app:'test1 b' test2:'frogs'" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuotedMultipleMixedStyle", "[app]") {
    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.allow_windows_style_options();
    app.parse(R"raw(/string:"this is my quoted string" /t 'qstring 2' -m=`"quoted string"`)raw");
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuotedMultipleInMiddle", "[app]") {
    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.parse(R"raw(--string="this is my quoted string" -t "qst\"ring 2" -m=`"quoted string"`)raw");
    CHECK("this is my quoted string" == str);
    CHECK("qst\"ring 2" == str2);
    CHECK("\"quoted string\"" == str3);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuotedEscapedCharacters", "[app]") {
    std::string str, str2, str3;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.parse(R"raw(--string="this is my \n\"quoted\" string" -t 'qst\ring 2' -m=`"quoted\n string"`")raw");
    CHECK("this is my \n\"quoted\" string" == str);  // escaped
    CHECK("qst\\ring 2" == str2);                    // literal
    CHECK("\"quoted\\n string\"" == str3);           // double quoted literal
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuotedMultipleWithEqual", "[app]") {
    std::string str, str2, str3, str4;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.add_option("-j,--jstr", str4);
    app.parse(R"raw(--string="this is my quoted string" -t 'qstring 2' -m=`"quoted string"` --jstr=Unquoted)raw");
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
    CHECK("Unquoted" == str4);
}

TEST_CASE_METHOD(TApp, "OneStringEqualVersionSingleStringQuotedMultipleWithEqualAndProgram", "[app]") {
    std::string str, str2, str3, str4;
    app.add_option("-s,--string", str);
    app.add_option("-t,--tstr", str2);
    app.add_option("-m,--mstr", str3);
    app.add_option("-j,--jstr", str4);
    app.parse(
        R"raw(program --string="this is my quoted string" -t 'qstring 2' -m=`"quoted string"` --jstr=Unquoted)raw",
        true);
    CHECK("this is my quoted string" == str);
    CHECK("qstring 2" == str2);
    CHECK("\"quoted string\"" == str3);
    CHECK("Unquoted" == str4);
}

TEST_CASE_METHOD(TApp, "OneStringFlagLike", "[app]") {
    std::string str{"something"};
    app.add_option("-s,--string", str)->expected(0, 1);
    args = {"--string"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK(str.empty());
}

TEST_CASE_METHOD(TApp, "OneIntFlagLike", "[app]") {
    int val{0};
    auto *opt = app.add_option("-i", val)->expected(0, 1);
    args = {"-i"};
    run();
    CHECK(app.count("-i") == 1u);
    opt->default_str("7");
    run();
    CHECK(7 == val);

    opt->default_val(9);
    run();
    CHECK(9 == val);
}

TEST_CASE_METHOD(TApp, "PairDefault", "[app]") {
    std::pair<double, std::string> pr{57.5, "test"};
    auto *opt = app.add_option("-i", pr)->expected(0, 2);
    args = {"-i"};
    run();
    CHECK(app.count("-i") == 1u);

    std::pair<double, std::string> pr2{92.5, "t2"};
    opt->default_val(pr2);
    run();
    CHECK(pr == pr2);
}

TEST_CASE_METHOD(TApp, "TupleDefault", "[app]") {
    std::tuple<double, std::string, int, std::string> pr{57.5, "test", 5, "total"};
    auto *opt = app.add_option("-i", pr)->expected(0, 4);
    args = {"-i"};
    run();
    CHECK(app.count("-i") == 1u);

    std::tuple<double, std::string, int, std::string> pr2{99.5, "test2", 87, "total3"};
    opt->default_val(pr2);
    run();
    CHECK(pr == pr2);
}

TEST_CASE_METHOD(TApp, "TupleDefaultSingle", "[app]") {
    std::tuple<std::string> pr{"test_tuple"};
    auto *opt = app.add_option("-i", pr)->expected(0, 1);
    args = {"-i"};
    run();
    CHECK(app.count("-i") == 1u);

    std::tuple<std::string> pr2{"total3"};
    opt->default_val(pr2);
    run();
    CHECK(pr == pr2);
}

TEST_CASE_METHOD(TApp, "TupleComplex", "[app]") {
    std::tuple<double, std::string, int, std::pair<std::string, std::string>> pr{57.5, "test", 5, {"total", "total2"}};
    auto *opt = app.add_option("-i", pr)->expected(0, 4);
    args = {"-i"};
    run();
    CHECK(app.count("-i") == 1u);

    std::tuple<double, std::string, int, std::pair<std::string, std::string>> pr2{
        99.5, "test2", 87, {"total3", "total4"}};
    opt->default_val(pr2);
    run();
    CHECK(pr == pr2);
}

TEST_CASE_METHOD(TApp, "invalidDefault", "[app]") {
    int pr{5};
    auto *opt = app.add_option("-i", pr)
                    ->expected(1)
                    ->multi_option_policy(CLI::MultiOptionPolicy::Throw)
                    ->delimiter(',')
                    ->force_callback();
    CHECK_THROWS(opt->default_val("4,6,2,8"));
}

TEST_CASE_METHOD(TApp, "TogetherInt", "[app]") {
    int i{0};
    app.add_option("-i,--int", i);
    args = {"-i4"};
    run();
    CHECK(app.count("--int") == 1u);
    CHECK(app.count("-i") == 1u);
    CHECK(4 == i);
    CHECK("4" == app["-i"]->as<std::string>());
    CHECK(4.0 == app["--int"]->as<double>());
}

TEST_CASE_METHOD(TApp, "SepInt", "[app]") {
    int i{0};
    app.add_option("-i,--int", i);
    args = {"-i", "4"};
    run();
    CHECK(app.count("--int") == 1u);
    CHECK(app.count("-i") == 1u);
    CHECK(4 == i);
}

TEST_CASE_METHOD(TApp, "DefaultStringAgain", "[app]") {
    std::string str = "previous";
    app.add_option("-s,--string", str);
    run();
    CHECK(app.count("-s") == 0u);
    CHECK(app.count("--string") == 0u);
    CHECK("previous" == str);
}

TEST_CASE_METHOD(TApp, "DefaultStringAgainEmpty", "[app]") {
    std::string str = "previous";
    app.add_option("-s,--string", str);
    app.parse("   ");
    CHECK(app.count("-s") == 0u);
    CHECK(app.count("--string") == 0u);
    CHECK("previous" == str);
}

TEST_CASE_METHOD(TApp, "DualOptions", "[app]") {

    std::string str = "previous";
    std::vector<std::string> vstr = {"previous"};
    std::vector<std::string> ans = {"one", "two"};
    app.add_option("-s,--string", str);
    app.add_option("-v,--vector", vstr);

    args = {"--vector=one", "--vector=two"};
    run();
    CHECK(vstr == ans);

    args = {"--string=one", "--string=two"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "LotsOfFlags", "[app]") {

    app.add_flag("-a");
    app.add_flag("-A");
    app.add_flag("-b");

    args = {"-a", "-b", "-aA"};
    run();
    CHECK(app.count("-a") == 2u);
    CHECK(app.count("-b") == 1u);
    CHECK(app.count("-A") == 1u);
    CHECK(4u == app.count_all());
}

TEST_CASE_METHOD(TApp, "NumberFlags", "[app]") {

    int val{0};
    app.add_flag("-1{1},-2{2},-3{3},-4{4},-5{5},-6{6}, -7{7}, -8{8}, -9{9}", val);

    args = {"-7"};
    run();
    CHECK(app.count("-1") == 1u);
    CHECK(7 == val);
}

TEST_CASE_METHOD(TApp, "doubleDashH", "[app]") {

    int val{0};
    // test you can add a --h option and it doesn't conflict with the help
    CHECK_NOTHROW(app.add_flag("--h", val));

    auto *topt = app.add_flag("-t");
    CHECK_THROWS_AS(app.add_flag("--t"), CLI::OptionAlreadyAdded);
    topt->configurable(false);
    CHECK_NOTHROW(app.add_flag("--t"));
}

TEST_CASE_METHOD(TApp, "DisableFlagOverrideTest", "[app]") {

    int val{0};
    auto *opt = app.add_flag("--1{1},--2{2},--3{3},--4{4},--5{5},--6{6}, --7{7}, --8{8}, --9{9}", val);
    CHECK(!opt->get_disable_flag_override());
    opt->disable_flag_override();
    args = {"--7=5"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
    CHECK(opt->get_disable_flag_override());
    opt->disable_flag_override(false);
    CHECK(!opt->get_disable_flag_override());
    CHECK_NOTHROW(run());
    CHECK(5 == val);
    opt->disable_flag_override();
    args = {"--7=7"};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(TApp, "LotsOfFlagsSingleString", "[app]") {

    app.add_flag("-a");
    app.add_flag("-A");
    app.add_flag("-b");

    app.parse("-a -b -aA");
    CHECK(app.count("-a") == 2u);
    CHECK(app.count("-b") == 1u);
    CHECK(app.count("-A") == 1u);
}

TEST_CASE_METHOD(TApp, "LotsOfFlagsSingleStringExtraSpace", "[app]") {

    app.add_flag("-a");
    app.add_flag("-A");
    app.add_flag("-b");

    app.parse("  -a    -b    -aA   ");
    CHECK(app.count("-a") == 2u);
    CHECK(app.count("-b") == 1u);
    CHECK(app.count("-A") == 1u);
}

TEST_CASE_METHOD(TApp, "SingleArgVector", "[app]") {

    std::vector<std::string> channels;
    std::vector<std::string> iargs;
    std::string path;
    app.add_option("-c", channels)->type_size(1)->allow_extra_args(false);
    app.add_option("args", iargs);
    app.add_option("-p", path);

    app.parse("-c t1 -c t2 -c t3 a1 a2 a3 a4 -p happy");
    CHECK(channels.size() == 3u);
    CHECK(iargs.size() == 4u);
    CHECK("happy" == path);

    app.parse("-c t1 a1 -c t2 -c t3 a2 a3 a4 -p happy");
    CHECK(channels.size() == 3u);
    CHECK(iargs.size() == 4u);
    CHECK("happy" == path);
}

TEST_CASE_METHOD(TApp, "StrangeOptionNames", "[app]") {
    app.add_option("-:");
    app.add_option("--t\tt");
    app.add_option("--{}");
    app.add_option("--:)");
    CHECK_THROWS_AS(app.add_option("--t t"), CLI::ConstructionError);
    args = {"-:)", "--{}", "5"};
    run();
    CHECK(app.count("-:") == 1u);
    CHECK(app.count("--{}") == 1u);
    CHECK(app["-:"]->as<char>() == ')');
    CHECK(app["--{}"]->as<int>() == 5);
}

TEST_CASE_METHOD(TApp, "singledash", "[app]") {
    app.add_option("-t");
    try {
        app.add_option("-test");
    } catch(const CLI::BadNameString &e) {
        std::string str = e.what();
        CHECK_THAT(str, Contains("2 dashes"));
        CHECK_THAT(str, Contains("-test"));
    } catch(...) {
        CHECK(false);
    }
    try {
        app.add_option("-!");
    } catch(const CLI::BadNameString &e) {
        std::string str = e.what();
        CHECK_THAT(str, Contains("one char"));
        CHECK_THAT(str, Contains("-!"));
    } catch(...) {
        CHECK(false);
    }
    app.allow_non_standard_option_names();
    try {
        app.add_option("-!I{am}bad");
    } catch(const CLI::BadNameString &e) {
        std::string str = e.what();
        CHECK_THAT(str, Contains("!I{am}bad"));
    } catch(...) {
        CHECK(false);
    }
}

TEST_CASE_METHOD(TApp, "FlagLikeOption", "[app]") {
    bool val{false};
    auto *opt = app.add_option("--flag", val)->type_size(0)->default_str("true");
    args = {"--flag"};
    run();
    CHECK(app.count("--flag") == 1u);
    CHECK(val);
    val = false;
    opt->type_size(0, 0);  // should be the same as above
    CHECK(0 == opt->get_type_size_min());
    CHECK(0 == opt->get_type_size_max());
    run();
    CHECK(app.count("--flag") == 1u);
    CHECK(val);
}

TEST_CASE_METHOD(TApp, "FlagLikeIntOption", "[app]") {
    int val{-47};
    auto *opt = app.add_option("--flag", val)->expected(0, 1);
    // normally some default value should be set, but this test is for some paths in the validators checks to skip
    // validation on empty string if nothing is expected
    opt->check(CLI::PositiveNumber);
    args = {"--flag"};
    CHECK(opt->as<std::string>().empty());
    run();
    CHECK(app.count("--flag") == 1u);
    CHECK(-47 != val);
    args = {"--flag", "12"};
    run();

    CHECK(12 == val);
    args.clear();
    run();
    CHECK(opt->as<std::string>().empty());
}

TEST_CASE_METHOD(TApp, "BoolOnlyFlag", "[app]") {
    bool bflag{false};
    app.add_flag("-b", bflag)->multi_option_policy(CLI::MultiOptionPolicy::Throw);

    args = {"-b"};
    REQUIRE_NOTHROW(run());
    CHECK(bflag);

    args = {"-b", "-b"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "ShortOpts", "[app]") {

    std::uint64_t funnyint{0};
    std::string someopt;
    app.add_flag("-z", funnyint);
    app.add_option("-y", someopt);

    args = {
        "-zzyzyz",
    };

    run();

    CHECK(app.count("-z") == 2u);
    CHECK(app.count("-y") == 1u);
    CHECK(funnyint == std::uint64_t{2});
    CHECK(someopt == "zyz");
    CHECK(3u == app.count_all());
}

TEST_CASE_METHOD(TApp, "TwoParamTemplateOpts", "[app]") {

    double funnyint{0.0};
    auto *opt = app.add_option<double, unsigned int>("-y", funnyint);

    args = {"-y", "32"};

    run();

    CHECK(funnyint == 32.0);

    args = {"-y", "32.3"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-y", "-19"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    opt->capture_default_str();
    CHECK(opt->get_default_str().empty());
}

TEST_CASE_METHOD(TApp, "DefaultOpts", "[app]") {

    int i{3};
    std::string s = "HI";

    app.add_option("-i,i", i);
    app.add_option("-s,s", s)->capture_default_str();  //  Used to be different

    args = {"-i2", "9"};

    run();

    CHECK(app.count("i") == 1u);
    CHECK(app.count("-s") == 1u);
    CHECK(i == 2);
    CHECK(s == "9");
}

TEST_CASE_METHOD(TApp, "TakeLastOpt", "[app]") {

    std::string str;
    app.add_option("--str", str)->multi_option_policy(CLI::MultiOptionPolicy::TakeLast);

    args = {"--str=one", "--str=two"};

    run();

    CHECK("two" == str);
}

TEST_CASE_METHOD(TApp, "TakeLastOpt2", "[app]") {

    std::string str;
    app.add_option("--str", str)->take_last();

    args = {"--str=one", "--str=two"};

    run();

    CHECK("two" == str);
}

TEST_CASE_METHOD(TApp, "TakeFirstOpt", "[app]") {

    std::string str;
    app.add_option("--str", str)->multi_option_policy(CLI::MultiOptionPolicy::TakeFirst);

    args = {"--str=one", "--str=two"};

    run();

    CHECK("one" == str);
}

TEST_CASE_METHOD(TApp, "TakeFirstOpt2", "[app]") {

    std::string str;
    app.add_option("--str", str)->take_first();

    args = {"--str=one", "--str=two"};

    run();

    CHECK("one" == str);
}

TEST_CASE_METHOD(TApp, "JoinOpt", "[app]") {

    std::string str;
    app.add_option("--str", str)->multi_option_policy(CLI::MultiOptionPolicy::Join);

    args = {"--str=one", "--str=two"};

    run();

    CHECK("one\ntwo" == str);
}

TEST_CASE_METHOD(TApp, "SumOpt", "[app]") {

    int val = 0;
    app.add_option("--val", val)->multi_option_policy(CLI::MultiOptionPolicy::Sum);

    args = {"--val=1", "--val=4"};

    run();

    CHECK(5 == val);
}

TEST_CASE_METHOD(TApp, "SumOptFloat", "[app]") {

    double val = NAN;
    app.add_option("--val", val)->multi_option_policy(CLI::MultiOptionPolicy::Sum);

    args = {"--val=1.3", "--val=-0.7"};

    run();

    CHECK(std::fabs(0.6 - val) <= std::numeric_limits<double>::epsilon());
}

TEST_CASE_METHOD(TApp, "SumOptString", "[app]") {

    std::string val;
    app.add_option("--val", val)->multi_option_policy(CLI::MultiOptionPolicy::Sum);

    args = {"--val=i", "--val=2"};

    run();

    CHECK("i2" == val);
}

TEST_CASE_METHOD(TApp, "ReverseOpt", "[app]") {

    std::vector<std::string> val;
    auto *opt1 = app.add_option("--val", val)->multi_option_policy(CLI::MultiOptionPolicy::Reverse);

    args = {"--val=string1", "--val=string2", "--val", "string3", "string4"};

    run();

    CHECK(val.size() == 4U);

    CHECK(val.front() == "string4");
    CHECK(val.back() == "string1");

    opt1->expected(1, 2);
    run();
    CHECK(val.size() == 2U);

    CHECK(val.front() == "string4");
    CHECK(val.back() == "string3");
    CHECK(opt1->get_multi_option_policy() == CLI::MultiOptionPolicy::Reverse);
}

TEST_CASE_METHOD(TApp, "JoinOpt2", "[app]") {

    std::string str;
    app.add_option("--str", str)->join();

    args = {"--str=one", "--str=two"};

    run();

    CHECK("one\ntwo" == str);
}

TEST_CASE_METHOD(TApp, "TakeLastOptMulti", "[app]") {
    std::vector<int> vals;
    app.add_option("--long", vals)->expected(2)->take_last();

    args = {"--long", "1", "2", "3"};

    run();

    CHECK(std::vector<int>({2, 3}) == vals);
}

TEST_CASE_METHOD(TApp, "TakeLastOptMulti_alternative_path", "[app]") {
    std::vector<int> vals;
    app.add_option("--long", vals)->expected(2, -1)->take_last();

    args = {"--long", "1", "2", "3"};

    run();

    CHECK(std::vector<int>({2, 3}) == vals);
}

TEST_CASE_METHOD(TApp, "TakeLastOptMultiCheck", "[app]") {
    std::vector<int> vals;
    auto *opt = app.add_option("--long", vals)->expected(-2)->take_last();

    opt->check(CLI::Validator(CLI::PositiveNumber).application_index(0));
    opt->check((!CLI::PositiveNumber).application_index(1));
    args = {"--long", "-1", "2", "-3"};

    CHECK_NOTHROW(run());

    CHECK(std::vector<int>({2, -3}) == vals);
}

TEST_CASE_METHOD(TApp, "TakeFirstOptMulti", "[app]") {
    std::vector<int> vals;
    app.add_option("--long", vals)->expected(2)->take_first();

    args = {"--long", "1", "2", "3"};

    run();

    CHECK(std::vector<int>({1, 2}) == vals);
}

TEST_CASE_METHOD(TApp, "ComplexOptMulti", "[app]") {
    std::complex<double> val;
    app.add_option("--long", val)->take_first()->allow_extra_args();

    args = {"--long", "1", "2", "3", "4"};

    run();

    CHECK(1 == Approx(val.real()));
    CHECK(2 == Approx(val.imag()));
}

TEST_CASE_METHOD(TApp, "MissingValueNonRequiredOpt", "[app]") {
    int count{0};
    app.add_option("-c,--count", count);

    args = {"-c"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--count"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "MissingValueMoreThan", "[app]") {
    std::vector<int> vals1;
    std::vector<int> vals2;
    app.add_option("-v", vals1)->expected(-2);
    app.add_option("--vals", vals2)->expected(-2);

    args = {"-v", "2"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--vals", "4"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "NoMissingValueMoreThan", "[app]") {
    std::vector<int> vals1;
    std::vector<int> vals2;
    app.add_option("-v", vals1)->expected(-2);
    app.add_option("--vals", vals2)->expected(-2);

    args = {"-v", "2", "3", "4"};
    run();
    CHECK(std::vector<int>({2, 3, 4}) == vals1);

    args = {"--vals", "2", "3", "4"};
    run();
    CHECK(std::vector<int>({2, 3, 4}) == vals2);
}

TEST_CASE_METHOD(TApp, "NotRequiredOptsSingle", "[app]") {

    std::string str;
    app.add_option("--str", str);

    args = {"--str"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "NotRequiredOptsSingleShort", "[app]") {

    std::string str;
    app.add_option("-s", str);

    args = {"-s"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "RequiredOptsSingle", "[app]") {

    std::string str;
    app.add_option("--str", str)->required();

    args = {"--str"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "RequiredOptsSingleShort", "[app]") {

    std::string str;
    app.add_option("-s", str)->required();

    args = {"-s"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "RequiredOptsDouble", "[app]") {

    std::vector<std::string> strs;
    app.add_option("--str", strs)->required()->expected(2);

    args = {"--str", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--str", "one", "two"};

    run();

    CHECK(std::vector<std::string>({"one", "two"}) == strs);
}

TEST_CASE_METHOD(TApp, "emptyVectorReturn", "[app]") {

    std::vector<std::string> strs;
    std::vector<std::string> strs2;
    std::vector<std::string> strs3;
    auto *opt1 = app.add_option("--str", strs)->required()->expected(0, 2);
    app.add_option("--str3", strs3)->expected(1, 3);
    app.add_option("--str2", strs2);
    args = {"--str"};

    CHECK_NOTHROW(run());
    CHECK(std::vector<std::string>({""}) == strs);
    args = {"--str", "one", "two"};

    run();

    CHECK(std::vector<std::string>({"one", "two"}) == strs);

    args = {"--str", "{}", "--str2", "{}"};

    run();

    CHECK(strs.empty());
    CHECK(std::vector<std::string>{"{}"} == strs2);
    opt1->default_str("{}");
    args = {"--str"};

    CHECK_NOTHROW(run());
    CHECK(strs.empty());
    opt1->required(false);
    args = {"--str3", "{}"};

    CHECK_NOTHROW(run());
    CHECK_FALSE(strs3.empty());
}

TEST_CASE_METHOD(TApp, "emptyVectorReturnReduce", "[app]") {

    std::vector<std::string> strs;
    std::vector<std::string> strs2;
    std::vector<std::string> strs3;
    auto *opt1 = app.add_option("--str", strs)->required()->expected(0, 2);
    app.add_option("--str3", strs3)->expected(1, 3);
    app.add_option("--str2", strs2)->expected(1, 1)->take_first();
    args = {"--str"};

    CHECK_NOTHROW(run());
    CHECK(std::vector<std::string>({""}) == strs);
    args = {"--str", "one", "two"};

    run();

    CHECK(std::vector<std::string>({"one", "two"}) == strs);

    args = {"--str", "{}", "--str2", "{}", "test"};

    run();

    CHECK(strs.empty());
    CHECK(std::vector<std::string>{"{}"} == strs2);
    opt1->default_str("{}");
    args = {"--str"};

    CHECK_NOTHROW(run());
    CHECK(strs.empty());
    opt1->required(false);
    args = {"--str3", "{}"};

    CHECK_NOTHROW(run());
    CHECK_FALSE(strs3.empty());
}

TEST_CASE_METHOD(TApp, "RequiredOptsDoubleShort", "[app]") {

    std::vector<std::string> strs;
    app.add_option("-s", strs)->required()->expected(2);

    args = {"-s", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"-s", "one", "-s", "one", "-s", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "RequiredOptsDoubleNeg", "[app]") {
    std::vector<std::string> strs;
    app.add_option("-s", strs)->required()->expected(-2);

    args = {"-s", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"-s", "one", "two", "-s", "three"};

    REQUIRE_NOTHROW(run());
    CHECK(std::vector<std::string>({"one", "two", "three"}) == strs);

    args = {"-s", "one", "two"};
    REQUIRE_NOTHROW(run());
    CHECK(std::vector<std::string>({"one", "two"}) == strs);
}

TEST_CASE_METHOD(TApp, "ExpectedRangeParam", "[app]") {

    app.add_option("-s")->required()->expected(2, 4);

    args = {"-s", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"-s", "one", "two"};

    CHECK_NOTHROW(run());

    args = {"-s", "one", "two", "three"};

    CHECK_NOTHROW(run());

    args = {"-s", "one", "two", "three", "four"};

    CHECK_NOTHROW(run());

    args = {"-s", "one", "two", "three", "four", "five"};

    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "ExpectedRangePositional", "[app]") {

    app.add_option("arg")->required()->expected(2, 4);

    args = {"one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"one", "two"};

    CHECK_NOTHROW(run());

    args = {"one", "two", "three"};

    CHECK_NOTHROW(run());

    args = {"one", "two", "three", "four"};

    CHECK_NOTHROW(run());

    args = {"one", "two", "three", "four", "five"};

    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

// This makes sure unlimited option priority is
// correct for space vs. no space #90
TEST_CASE_METHOD(TApp, "PositionalNoSpace", "[app]") {
    std::vector<std::string> options;
    std::string foo, bar;

    app.add_option("-O", options);
    app.add_option("foo", foo)->required();
    app.add_option("bar", bar)->required();

    args = {"-O", "Test", "param1", "param2"};
    run();

    CHECK(1u == options.size());
    CHECK("Test" == options.at(0));

    args = {"-OTest", "param1", "param2"};
    run();

    CHECK(1u == options.size());
    CHECK("Test" == options.at(0));
}

// Tests positionals at end
TEST_CASE_METHOD(TApp, "PositionalAtEnd", "[app]") {
    std::string options;
    std::string foo;

    app.add_option("-O", options);
    app.add_option("foo", foo);
    app.positionals_at_end();
    CHECK(app.get_positionals_at_end());
    args = {"-O", "Test", "param1"};
    run();

    CHECK("Test" == options);
    CHECK("param1" == foo);

    args = {"param2", "-O", "Test"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

// Tests positionals at end
TEST_CASE_METHOD(TApp, "PositionalInjectSeparator", "[app]") {
    std::string options;
    std::vector<std::vector<std::string>> foo;

    app.add_option("-O", options);
    auto *fooopt = app.add_option("foo", foo);
    fooopt->inject_separator();
    args = {"test1", "-O", "Test", "test2"};
    run();

    CHECK("Test" == options);
    CHECK(foo.size() == 2U);
}

// Tests positionals at end
TEST_CASE_METHOD(TApp, "RequiredPositionals", "[app]") {
    std::vector<std::string> sources;
    std::string dest;
    app.add_option("src", sources);
    app.add_option("dest", dest)->required();
    app.positionals_at_end();

    args = {"1", "2", "3"};
    run();

    CHECK(2u == sources.size());
    CHECK("3" == dest);

    args = {"a"};
    sources.clear();
    run();

    CHECK(sources.empty());
    CHECK("a" == dest);
}

TEST_CASE_METHOD(TApp, "RequiredPositionalVector", "[app]") {
    std::string d1;
    std::string d2;
    std::string d3;
    std::vector<std::string> sources;

    app.add_option("dest1", d1);
    app.add_option("dest2", d2);
    app.add_option("dest3", d3);
    app.add_option("src", sources)->required();

    app.positionals_at_end();

    args = {"1", "2", "3"};
    run();

    CHECK(1u == sources.size());
    CHECK("1" == d1);
    CHECK("2" == d2);
    CHECK(d3.empty());
    args = {"a"};
    sources.clear();
    run();

    CHECK(1u == sources.size());
}

// Tests positionals at end
TEST_CASE_METHOD(TApp, "RequiredPositionalValidation", "[app]") {
    std::vector<std::string> sources;
    int dest = 0;  // required
    std::string d2;
    app.add_option("src", sources);
    app.add_option("dest", dest)->required()->check(CLI::PositiveNumber);
    app.add_option("dest2", d2)->required();
    app.positionals_at_end()->validate_positionals();

    args = {"1", "2", "string", "3"};
    run();

    CHECK(2u == sources.size());
    CHECK(3 == dest);
    CHECK("string" == d2);
}

// Tests positionals at end
TEST_CASE_METHOD(TApp, "PositionalValidation", "[app]") {
    std::string options;
    std::string foo;

    app.add_option("bar", options)->check(CLI::Number.name("valbar"));
    // disable the check on foo
    app.add_option("foo", foo)->check(CLI::Number.active(false));
    app.validate_positionals();
    args = {"1", "param1"};
    run();

    CHECK("1" == options);
    CHECK("param1" == foo);

    args = {"param1", "1"};
    CHECK_NOTHROW(run());

    CHECK("1" == options);
    CHECK("param1" == foo);

    CHECK(nullptr != app.get_option("bar")->get_validator("valbar"));
}

TEST_CASE_METHOD(TApp, "PositionalNoSpaceLong", "[app]") {
    std::vector<std::string> options;
    std::string foo, bar;

    app.add_option("--option", options);
    app.add_option("foo", foo)->required();
    app.add_option("bar", bar)->required();

    args = {"--option", "Test", "param1", "param2"};
    run();

    CHECK(1u == options.size());
    CHECK("Test" == options.at(0));

    args = {"--option=Test", "param1", "param2"};
    run();

    CHECK(1u == options.size());
    CHECK("Test" == options.at(0));
}

TEST_CASE_METHOD(TApp, "RequiredOptsUnlimited", "[app]") {

    std::vector<std::string> strs;
    app.add_option("--str", strs)->required();

    args = {"--str"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--str", "one", "--str", "two"};
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);

    args = {"--str", "one", "two"};
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);

    // It's better to feed a hungry option than to feed allow_extras
    app.allow_extras();
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);
    CHECK(std::vector<std::string>({}) == app.remaining());

    app.allow_extras(false);
    std::vector<std::string> remain;
    auto *popt = app.add_option("positional", remain);
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);
    CHECK(remain.empty());

    args = {"--str", "one", "--", "two"};

    run();
    CHECK(std::vector<std::string>({"one"}) == strs);
    CHECK(std::vector<std::string>({"two"}) == remain);

    args = {"one", "--str", "two"};

    run();
    CHECK(std::vector<std::string>({"two"}) == strs);
    CHECK(std::vector<std::string>({"one"}) == remain);

    args = {"--str", "one", "two"};
    popt->required();
    run();
    CHECK(std::vector<std::string>({"one"}) == strs);
    CHECK(std::vector<std::string>({"two"}) == remain);
}

TEST_CASE_METHOD(TApp, "RequiredOptsUnlimitedShort", "[app]") {

    std::vector<std::string> strs;
    app.add_option("-s", strs)->required();

    args = {"-s"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"-s", "one", "-s", "two"};
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);

    args = {"-s", "one", "two"};
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);

    // It's better to feed a hungry option than to feed allow_extras
    app.allow_extras();
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);
    CHECK(std::vector<std::string>({}) == app.remaining());

    app.allow_extras(false);
    std::vector<std::string> remain;
    app.add_option("positional", remain);
    run();
    CHECK(std::vector<std::string>({"one", "two"}) == strs);
    CHECK(remain.empty());

    args = {"-s", "one", "--", "two"};

    run();
    CHECK(std::vector<std::string>({"one"}) == strs);
    CHECK(std::vector<std::string>({"two"}) == remain);

    args = {"one", "-s", "two"};

    run();
    CHECK(std::vector<std::string>({"two"}) == strs);
    CHECK(std::vector<std::string>({"one"}) == remain);
}

TEST_CASE_METHOD(TApp, "OptsUnlimitedEnd", "[app]") {
    std::vector<std::string> strs;
    app.add_option("-s,--str", strs);
    app.allow_extras();

    args = {"one", "-s", "two", "three", "--", "four"};

    run();

    CHECK(std::vector<std::string>({"two", "three"}) == strs);
    CHECK(std::vector<std::string>({"one", "four"}) == app.remaining());
}

TEST_CASE_METHOD(TApp, "RequireOptPriority", "[app]") {

    std::vector<std::string> strs;
    app.add_option("--str", strs);
    std::vector<std::string> remain;
    app.add_option("positional", remain)->expected(2)->required();

    args = {"--str", "one", "two", "three"};
    run();

    CHECK(std::vector<std::string>({"one"}) == strs);
    CHECK(std::vector<std::string>({"two", "three"}) == remain);

    args = {"two", "three", "--str", "one", "four"};
    run();

    CHECK(std::vector<std::string>({"one", "four"}) == strs);
    CHECK(std::vector<std::string>({"two", "three"}) == remain);
}

TEST_CASE_METHOD(TApp, "RequireOptPriorityShort", "[app]") {

    std::vector<std::string> strs;
    app.add_option("-s", strs)->required();
    std::vector<std::string> remain;
    app.add_option("positional", remain)->expected(2)->required();

    args = {"-s", "one", "two", "three"};
    run();

    CHECK(std::vector<std::string>({"one"}) == strs);
    CHECK(std::vector<std::string>({"two", "three"}) == remain);

    args = {"two", "three", "-s", "one", "four"};
    run();

    CHECK(std::vector<std::string>({"one", "four"}) == strs);
    CHECK(std::vector<std::string>({"two", "three"}) == remain);
}

TEST_CASE_METHOD(TApp, "NotRequiredExpectedDouble", "[app]") {

    std::vector<std::string> strs;
    app.add_option("--str", strs)->expected(2);

    args = {"--str", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "NotRequiredExpectedDoubleShort", "[app]") {

    std::vector<std::string> strs;
    app.add_option("-s", strs)->expected(2);

    args = {"-s", "one"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "RequiredFlags", "[app]") {
    app.add_flag("-a")->required();
    app.add_flag("-b")->mandatory();  // Alternate term

    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"-a"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"-b"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"-a", "-b"};
    run();
}

TEST_CASE_METHOD(TApp, "CallbackFlags", "[app]") {

    std::int64_t value{0};

    auto func = [&value](std::int64_t x) { value = x; };

    app.add_flag_function("-v", func);

    run();
    CHECK(0u == value);

    args = {"-v"};
    run();
    CHECK(1u == value);

    args = {"-vv"};
    run();
    CHECK(2u == value);

    CHECK_THROWS_AS(app.add_flag_function("hi", func), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "CallbackFlagsFalse", "[app]") {
    std::int64_t value = 0;

    auto func = [&value](std::int64_t x) { value = x; };

    app.add_flag_function("-v,-f{false},--val,--fval{false}", func);

    run();
    CHECK(0 == value);

    args = {"-f"};
    run();
    CHECK(-1 == value);

    args = {"-vfv"};
    run();
    CHECK(1 == value);

    args = {"--fval"};
    run();
    CHECK(-1 == value);

    args = {"--fval=2"};
    run();
    CHECK(-2 == value);

    CHECK_THROWS_AS(app.add_flag_function("hi", func), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "CallbackFlagsFalseShortcut", "[app]") {
    std::int64_t value = 0;

    auto func = [&value](std::int64_t x) { value = x; };

    app.add_flag_function("-v,!-f,--val,!--fval", func);

    run();
    CHECK(0 == value);

    args = {"-f"};
    run();
    CHECK(-1 == value);

    args = {"-vfv"};
    run();
    CHECK(1 == value);

    args = {"--fval"};
    run();
    CHECK(-1 == value);

    args = {"--fval=2"};
    run();
    CHECK(-2 == value);

    CHECK_THROWS_AS(app.add_flag_function("hi", func), CLI::IncorrectConstruction);
}

#if __cplusplus >= 201402L || _MSC_VER >= 1900
TEST_CASE_METHOD(TApp, "CallbackFlagsAuto", "[app]") {

    std::int64_t value{0};

    auto func = [&value](std::int64_t x) { value = x; };

    app.add_flag("-v", func);

    run();
    CHECK(0u == value);

    args = {"-v"};
    run();
    CHECK(1u == value);

    args = {"-vv"};
    run();
    CHECK(2u == value);

    CHECK_THROWS_AS(app.add_flag("hi", func), CLI::IncorrectConstruction);
}
#endif

TEST_CASE_METHOD(TApp, "Positionals", "[app]") {

    std::string posit1;
    std::string posit2;
    app.add_option("posit1", posit1);
    app.add_option("posit2", posit2);

    args = {"thing1", "thing2"};

    run();

    CHECK(app.count("posit1") == 1u);
    CHECK(app.count("posit2") == 1u);
    CHECK(posit1 == "thing1");
    CHECK(posit2 == "thing2");
}

TEST_CASE_METHOD(TApp, "ForcedPositional", "[app]") {
    std::vector<std::string> posit;
    auto *one = app.add_flag("--one");
    app.add_option("posit", posit);

    args = {"--one", "two", "three"};
    run();
    std::vector<std::string> answers1 = {"two", "three"};
    CHECK(one->count());
    CHECK(posit == answers1);

    args = {"--", "--one", "two", "three"};
    std::vector<std::string> answers2 = {"--one", "two", "three"};
    run();

    CHECK(!one->count());
    CHECK(posit == answers2);
}

TEST_CASE_METHOD(TApp, "MixedPositionals", "[app]") {

    int positional_int{0};
    std::string positional_string;
    app.add_option("posit1,--posit1", positional_int, "");
    app.add_option("posit2,--posit2", positional_string, "");

    args = {"--posit2", "thing2", "7"};

    run();

    CHECK(app.count("posit2") == 1u);
    CHECK(app.count("--posit1") == 1u);
    CHECK(positional_int == 7);
    CHECK(positional_string == "thing2");
}

TEST_CASE_METHOD(TApp, "BigPositional", "[app]") {
    std::vector<std::string> vec;
    app.add_option("pos", vec);

    args = {"one"};

    run();
    CHECK(vec == args);

    args = {"one", "two"};
    run();

    CHECK(vec == args);
}

TEST_CASE_METHOD(TApp, "VectorArgAndPositional", "[app]") {
    std::vector<std::string> vec;
    std::vector<int> ivec;
    app.add_option("pos", vec);
    app.add_option("--args", ivec)->check(CLI::Number);
    app.validate_optional_arguments();
    args = {"one"};

    run();
    CHECK(vec == args);

    args = {"--args", "1", "2"};

    run();
    CHECK(ivec.size() == 2);
    vec.clear();
    ivec.clear();

    args = {"--args", "1", "2", "one", "two"};
    run();

    CHECK(vec.size() == 2);
    CHECK(ivec.size() == 2);

    app.validate_optional_arguments(false);
    CHECK_THROWS(run());
}

TEST_CASE_METHOD(TApp, "Reset", "[app]") {

    app.add_flag("--simple");
    double doub{0.0};
    app.add_option("-d,--double", doub);

    args = {"--simple", "--double", "1.2"};

    run();

    CHECK(app.count("--simple") == 1u);
    CHECK(app.count("-d") == 1u);
    CHECK(doub == Approx(1.2));

    app.clear();

    CHECK(app.count("--simple") == 0u);
    CHECK(app.count("-d") == 0u);

    run();

    CHECK(app.count("--simple") == 1u);
    CHECK(app.count("-d") == 1u);
    CHECK(doub == Approx(1.2));
}

TEST_CASE_METHOD(TApp, "RemoveOption", "[app]") {
    app.add_flag("--one");
    auto *opt = app.add_flag("--two");

    CHECK(app.remove_option(opt));
    CHECK(!app.remove_option(opt));

    args = {"--two"};

    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "RemoveNeedsLinks", "[app]") {
    auto *one = app.add_flag("--one");
    auto *two = app.add_flag("--two");

    two->needs(one);
    one->needs(two);

    CHECK(app.remove_option(one));

    args = {"--two"};

    run();
}

TEST_CASE_METHOD(TApp, "RemoveExcludesLinks", "[app]") {
    auto *one = app.add_flag("--one");
    auto *two = app.add_flag("--two");

    two->excludes(one);
    one->excludes(two);

    CHECK(app.remove_option(one));

    args = {"--two"};

    run();  // Mostly hoping it does not crash
}

TEST_CASE_METHOD(TApp, "FileNotExists", "[app]") {
    std::string myfile{"TestNonFileNotUsed.txt"};
    REQUIRE_NOTHROW(CLI::NonexistentPath(myfile));

    std::string filename;
    auto *opt = app.add_option("--file", filename)->check(CLI::NonexistentPath, "path_check");
    args = {"--file", myfile};

    run();
    CHECK(filename == myfile);

    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    CHECK(ok);
    CHECK_THROWS_AS(run(), CLI::ValidationError);
    // deactivate the check, so it should run now
    opt->get_validator("path_check")->active(false);
    CHECK_NOTHROW(run());
    std::remove(myfile.c_str());
    CHECK(!CLI::ExistingFile(myfile).empty());
}

TEST_CASE_METHOD(TApp, "FileExists", "[app]") {
    std::string myfile{"TestNonFileNotUsed.txt"};
    CHECK(!CLI::ExistingFile(myfile).empty());

    std::string filename = "Failed";
    app.add_option("--file", filename)->check(CLI::ExistingFile);
    args = {"--file", myfile};

    CHECK_THROWS_AS(run(), CLI::ValidationError);

    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    CHECK(ok);
    run();
    CHECK(filename == myfile);

    std::remove(myfile.c_str());
    CHECK(!CLI::ExistingFile(myfile).empty());
}

#if defined CLI11_HAS_FILESYSTEM && CLI11_HAS_FILESYSTEM > 0 && defined(_MSC_VER)
TEST_CASE_METHOD(TApp, "filesystemWideName", "[app]") {
    std::filesystem::path myfile{L"voil\u20ac.txt"};

    std::filesystem::path fpath;
    app.add_option("--file", fpath)->check(CLI::ExistingFile, "existing file");

    CHECK_THROWS_AS(app.parse(L"--file voil\u20ac.txt"), CLI::ValidationError);

    bool ok = static_cast<bool>(std::ofstream(myfile).put('a'));  // create file
    CHECK(ok);

    // deactivate the check, so it should run now

    CHECK_NOTHROW(app.parse(L"--file voil\u20ac.txt"));

    CHECK(fpath == myfile);

    CHECK(std::filesystem::exists(fpath));
    std::filesystem::remove(myfile);
    CHECK(!std::filesystem::exists(fpath));
}
#endif

TEST_CASE_METHOD(TApp, "NotFileExists", "[app]") {
    std::string myfile{"TestNonFileNotUsed.txt"};
    CHECK(!CLI::ExistingFile(myfile).empty());

    std::string filename = "Failed";
    app.add_option("--file", filename)->check(!CLI::ExistingFile);
    args = {"--file", myfile};

    CHECK_NOTHROW(run());

    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    CHECK(ok);
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    std::remove(myfile.c_str());
    CHECK(!CLI::ExistingFile(myfile).empty());
}

TEST_CASE_METHOD(TApp, "DefaultedResult", "[app]") {
    std::string sval = "NA";
    int ival{0};
    auto *opts = app.add_option("--string", sval)->capture_default_str();
    auto *optv = app.add_option("--val", ival);
    args = {};
    run();
    CHECK("NA" == sval);
    std::string nString;
    opts->results(nString);
    CHECK("NA" == nString);
    int newIval = 0;
    // CHECK_THROWS_AS (optv->results(newIval), CLI::ConversionError);
    optv->default_str("442");
    optv->results(newIval);
    CHECK(442 == newIval);
}

TEST_CASE_METHOD(TApp, "OriginalOrder", "[app]") {
    std::vector<int> st1;
    CLI::Option *op1 = app.add_option("-a", st1);
    std::vector<int> st2;
    CLI::Option *op2 = app.add_option("-b", st2);

    args = {"-a", "1", "-b", "2", "-a3", "-a", "4"};

    run();

    CHECK(std::vector<int>({1, 3, 4}) == st1);
    CHECK(std::vector<int>({2}) == st2);

    CHECK(std::vector<CLI::Option *>({op1, op2, op1, op1}) == app.parse_order());
}

TEST_CASE_METHOD(TApp, "NeedsFlags", "[app]") {
    CLI::Option *opt = app.add_flag("-s,--string");
    app.add_flag("--both")->needs(opt);

    run();

    args = {"-s"};
    run();

    args = {"-s", "--both"};
    run();

    args = {"--both"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    CHECK_NOTHROW(opt->needs(opt));
}

TEST_CASE_METHOD(TApp, "ExcludesFlags", "[app]") {
    CLI::Option *opt = app.add_flag("-s,--string");
    app.add_flag("--nostr")->excludes(opt);

    run();

    args = {"-s"};
    run();

    args = {"--nostr"};
    run();

    args = {"--nostr", "-s"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);

    args = {"--string", "--nostr"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);

    CHECK_THROWS_AS(opt->excludes(opt), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "ExcludesMixedFlags", "[app]") {
    CLI::Option *opt1 = app.add_flag("--opt1");
    app.add_flag("--opt2");
    CLI::Option *opt3 = app.add_flag("--opt3");
    app.add_flag("--no")->excludes(opt1, "--opt2", opt3);

    run();

    args = {"--no"};
    run();

    args = {"--opt2"};
    run();

    args = {"--no", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);

    args = {"--no", "--opt2"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);
}

TEST_CASE_METHOD(TApp, "NeedsMultiFlags", "[app]") {
    CLI::Option *opt1 = app.add_flag("--opt1");
    CLI::Option *opt2 = app.add_flag("--opt2");
    CLI::Option *opt3 = app.add_flag("--opt3");
    app.add_flag("--optall")->needs(opt1, opt2, opt3);  // NOLINT(readability-suspicious-call-argument)

    run();

    args = {"--opt1"};
    run();

    args = {"--opt2"};
    run();

    args = {"--optall"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--optall", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--optall", "--opt2", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--optall", "--opt1", "--opt2", "--opt3"};
    run();
}

TEST_CASE_METHOD(TApp, "NeedsMixedFlags", "[app]") {
    CLI::Option *opt1 = app.add_flag("--opt1");
    app.add_flag("--opt2");
    app.add_flag("--opt3");
    app.add_flag("--optall")->needs(opt1, "--opt2", "--opt3");

    run();

    args = {"--opt1"};
    run();

    args = {"--opt2"};
    run();

    args = {"--optall"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--optall", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--optall", "--opt2", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--optall", "--opt1", "--opt2", "--opt3"};
    run();
}

TEST_CASE_METHOD(TApp, "NeedsChainedFlags", "[app]") {
    CLI::Option *opt1 = app.add_flag("--opt1");
    CLI::Option *opt2 = app.add_flag("--opt2")->needs(opt1);
    app.add_flag("--opt3")->needs(opt2);

    run();

    args = {"--opt1"};
    run();

    args = {"--opt2"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--opt3"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--opt3", "--opt2"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--opt3", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--opt2", "--opt1"};
    run();

    args = {"--opt1", "--opt2", "--opt3"};
    run();
}

TEST_CASE_METHOD(TApp, "Env", "[app]") {

    put_env("CLI11_TEST_ENV_TMP", "2");

    int val{1};
    CLI::Option *vopt = app.add_option("--tmp", val)->envname("CLI11_TEST_ENV_TMP");

    run();

    CHECK(val == 2);
    CHECK(vopt->count() == 1u);

    vopt->required();
    run();

    unset_env("CLI11_TEST_ENV_TMP");
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

// curiously check if an environmental only option works
TEST_CASE_METHOD(TApp, "EnvOnly", "[app]") {

    put_env("CLI11_TEST_ENV_TMP", "2");

    int val{1};
    CLI::Option *vopt = app.add_option("", val)->envname("CLI11_TEST_ENV_TMP");

    run();

    CHECK(val == 2);
    CHECK(vopt->count() == 1u);

    vopt->required();
    run();

    unset_env("CLI11_TEST_ENV_TMP");
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

// reported bug #1013 on github
TEST_CASE_METHOD(TApp, "groupEnvRequired", "[app]") {
    std::string str;
    auto *group1 = app.add_option_group("group1");
    put_env("CLI11_TEST_GROUP_REQUIRED", "string_abc");
    group1->add_option("-f", str, "f")->envname("CLI11_TEST_GROUP_REQUIRED")->required();

    run();
    CHECK(str == "string_abc");
    unset_env("CLI11_TEST_GROUP_REQUIRED");
}

TEST_CASE_METHOD(TApp, "RangeInt", "[app]") {
    int x{0};
    app.add_option("--one", x)->check(CLI::Range(3, 6));

    args = {"--one=1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=7"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=3"};
    run();

    args = {"--one=5"};
    run();

    args = {"--one=6"};
    run();
}

TEST_CASE_METHOD(TApp, "RangeDouble", "[app]") {

    double x{0.0};
    /// Note that this must be a double in Range, too
    app.add_option("--one", x)->check(CLI::Range(3.0, 6.0));

    args = {"--one=1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=7"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=3"};
    run();

    args = {"--one=5"};
    run();

    args = {"--one=6"};
    run();
}

TEST_CASE_METHOD(TApp, "RangeFloat", "[app]") {

    float x{0.0f};
    /// Note that this must be a float in Range, too
    app.add_option("--one", x, "testing floats")->check(CLI::Range(3.0, 6.0));

    args = {"--one=1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=7"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=3"};
    run();

    args = {"--one=5"};
    run();

    args = {"--one=6"};
    run();
}

TEST_CASE_METHOD(TApp, "NonNegative", "[app]") {

    std::string res;
    /// Note that this must be a double in Range, too
    app.add_option("--one", res)->check(CLI::NonNegativeNumber);

    args = {"--one=crazy"};
    try {
        // this should throw
        run();
        CHECK(false);
    } catch(const CLI::ValidationError &e) {
        std::string emess = e.what();
        CHECK(emess.size() < 70U);
    }
}

TEST_CASE_METHOD(TApp, "typeCheck", "[app]") {

    /// Note that this must be a double in Range, too
    app.add_option("--one")->check(CLI::TypeValidator<unsigned int>());

    args = {"--one=1"};
    CHECK_NOTHROW(run());

    args = {"--one=-7"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=error"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--one=4.568"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "NeedsTrue", "[app]") {
    std::string str;
    app.add_option("-s,--string", str);
    app.add_flag("--opt1")->check([&](const std::string &) {
        return (str != "val_with_opt1") ? std::string("--opt1 requires --string val_with_opt1") : std::string{};
    });

    run();

    args = {"--opt1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--string", "val"};
    run();

    args = {"--string", "val", "--opt1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--string", "val_with_opt1", "--opt1"};
    run();

    args = {"--opt1", "--string", "val_with_opt1"};  // order is not relevant
    run();
}

// Check to make sure programmatic access to left over is available
TEST_CASE_METHOD(TApp, "AllowExtras", "[app]") {

    app.allow_extras();

    bool val{true};
    app.add_flag("-f", val);

    args = {"-x", "-f"};

    REQUIRE_NOTHROW(run());
    CHECK(val);
    CHECK(std::vector<std::string>({"-x"}) == app.remaining());
}

TEST_CASE_METHOD(TApp, "AllowExtrasOrder", "[app]") {

    app.allow_extras();

    args = {"-x", "-f"};
    REQUIRE_NOTHROW(run());
    CHECK(std::vector<std::string>({"-x", "-f"}) == app.remaining());

    std::vector<std::string> left_over = app.remaining();
    app.parse(left_over);
    CHECK(std::vector<std::string>({"-f", "-x"}) == app.remaining());
    CHECK(left_over == app.remaining_for_passthrough());
}

TEST_CASE_METHOD(TApp, "AllowExtrasCascade", "[app]") {

    app.allow_extras();

    args = {"-x", "45", "-f", "27"};
    REQUIRE_NOTHROW(run());
    CHECK(std::vector<std::string>({"-x", "45", "-f", "27"}) == app.remaining());

    std::vector<std::string> left_over = app.remaining_for_passthrough();

    CLI::App capp{"cascade_program"};
    int v1 = 0;
    int v2 = 0;
    capp.add_option("-x", v1);
    capp.add_option("-f", v2);

    capp.parse(left_over);
    CHECK(45 == v1);
    CHECK(27 == v2);
}
// makes sure the error throws on the rValue version of the parse
TEST_CASE_METHOD(TApp, "ExtrasErrorRvalueParse", "[app]") {

    args = {"-x", "45", "-f", "27"};
    CHECK_THROWS_AS(app.parse(std::vector<std::string>({"-x", "45", "-f", "27"})), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "AllowExtrasCascadeDirect", "[app]") {

    app.allow_extras();

    args = {"-x", "45", "-f", "27"};
    REQUIRE_NOTHROW(run());
    CHECK(std::vector<std::string>({"-x", "45", "-f", "27"}) == app.remaining());

    CLI::App capp{"cascade_program"};
    int v1{0};
    int v2{0};
    capp.add_option("-x", v1);
    capp.add_option("-f", v2);

    capp.parse(app.remaining_for_passthrough());
    CHECK(45 == v1);
    CHECK(27 == v2);
}

TEST_CASE_METHOD(TApp, "AllowExtrasArgModify", "[app]") {

    int v1{0};
    int v2{0};
    app.allow_extras();
    app.add_option("-f", v2);
    args = {"27", "-f", "45", "-x"};
    app.parse(args);
    CHECK(std::vector<std::string>({"45", "-x"}) == args);

    CLI::App capp{"cascade_program"};

    capp.add_option("-x", v1);

    capp.parse(args);
    CHECK(45 == v1);
    CHECK(27 == v2);
}

// Test horrible error
TEST_CASE_METHOD(TApp, "CheckShortFail", "[app]") {
    args = {"--two"};

    CHECK_THROWS_AS(CLI::detail::AppFriend::parse_arg(&app, args, CLI::detail::Classifier::SHORT, false),
                    CLI::HorribleError);
}

// Test horrible error
TEST_CASE_METHOD(TApp, "CheckLongFail", "[app]") {
    args = {"-t"};

    CHECK_THROWS_AS(CLI::detail::AppFriend::parse_arg(&app, args, CLI::detail::Classifier::LONG, false),
                    CLI::HorribleError);
}

// Test horrible error
TEST_CASE_METHOD(TApp, "CheckWindowsFail", "[app]") {
    args = {"-t"};

    CHECK_THROWS_AS(CLI::detail::AppFriend::parse_arg(&app, args, CLI::detail::Classifier::WINDOWS_STYLE, false),
                    CLI::HorribleError);
}

// Test horrible error
TEST_CASE_METHOD(TApp, "CheckOtherFail", "[app]") {
    args = {"-t"};

    CHECK_THROWS_AS(CLI::detail::AppFriend::parse_arg(&app, args, CLI::detail::Classifier::NONE, false),
                    CLI::HorribleError);
}

// Test horrible error
TEST_CASE_METHOD(TApp, "CheckSubcomFail", "[app]") {
    args = {"subcom"};

    CHECK_THROWS_AS(CLI::detail::AppFriend::parse_subcommand(&app, args), CLI::HorribleError);
}

TEST_CASE_METHOD(TApp, "FallthroughParentFail", "[app]") {
    CHECK_THROWS_AS(CLI::detail::AppFriend::get_fallthrough_parent(&app), CLI::HorribleError);
}

TEST_CASE_METHOD(TApp, "FallthroughParents", "[app]") {
    auto *sub = app.add_subcommand("test");
    CHECK(&app == CLI::detail::AppFriend::get_fallthrough_parent(sub));

    auto *ssub = sub->add_subcommand("sub2");
    CHECK(sub == CLI::detail::AppFriend::get_fallthrough_parent(ssub));

    auto *og1 = app.add_option_group("g1");
    auto *og2 = og1->add_option_group("g2");
    auto *og3 = og2->add_option_group("g3");
    CHECK(&app == CLI::detail::AppFriend::get_fallthrough_parent(og3));

    auto *ogb1 = sub->add_option_group("g1");
    auto *ogb2 = ogb1->add_option_group("g2");
    auto *ogb3 = ogb2->add_option_group("g3");
    CHECK(sub == CLI::detail::AppFriend::get_fallthrough_parent(ogb3));

    ogb2->name("groupb");
    CHECK(ogb2 == CLI::detail::AppFriend::get_fallthrough_parent(ogb3));
}

TEST_CASE_METHOD(TApp, "OptionWithDefaults", "[app]") {
    int someint{2};
    app.add_option("-a", someint)->capture_default_str();

    args = {"-a1", "-a2"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

// Added to test ->transform
TEST_CASE_METHOD(TApp, "OrderedModifyingTransforms", "[app]") {
    std::vector<std::string> val;
    auto *m = app.add_option("-m", val);
    m->transform([](std::string x) { return x + "1"; });
    m->transform([](std::string x) { return x + "2"; });

    args = {"-mone", "-mtwo"};

    run();

    CHECK(std::vector<std::string>({"one21", "two21"}) == val);
}

// non standard options
TEST_CASE_METHOD(TApp, "nonStandardOptions", "[app]") {
    std::string string1;
    CHECK_THROWS_AS(app.add_option("-single", string1), CLI::BadNameString);
    app.allow_non_standard_option_names();
    CHECK(app.get_allow_non_standard_option_names());
    app.add_option("-single", string1);
    args = {"-single", "string1"};

    run();

    CHECK(string1 == "string1");
}

TEST_CASE_METHOD(TApp, "nonStandardOptions2", "[app]") {
    std::vector<std::string> strings;
    app.allow_non_standard_option_names();
    app.add_option("-single,--single,-m", strings);
    args = {"-single", "string1", "--single", "string2"};

    run();

    CHECK(strings == std::vector<std::string>{"string1", "string2"});
}

TEST_CASE_METHOD(TApp, "nonStandardOptionsIntersect", "[app]") {
    std::vector<std::string> strings;
    app.allow_non_standard_option_names();
    app.add_option("-s,-t");
    CHECK_THROWS_AS(app.add_option("-single,--single", strings), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "nonStandardOptionsIntersect2", "[app]") {
    std::vector<std::string> strings;
    app.allow_non_standard_option_names();
    app.add_option("-single,--single", strings);
    CHECK_THROWS_AS(app.add_option("-s,-t"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "ThrowingTransform", "[app]") {
    std::string val;
    auto *m = app.add_option("-m,--mess", val);
    m->transform([](std::string) -> std::string { throw CLI::ValidationError("My Message"); });

    REQUIRE_NOTHROW(run());

    args = {"-mone"};

    REQUIRE_THROWS_AS(run(), CLI::ValidationError);

    try {
        run();
    } catch(const CLI::ValidationError &e) {
        CHECK(std::string("--mess: My Message") == e.what());
    }
}

// This was added to make running a simple function on each item easier
TEST_CASE_METHOD(TApp, "EachItem", "[app]") {

    std::vector<std::string> results;
    std::vector<std::string> dummy;

    auto *opt = app.add_option("--vec", dummy);

    opt->each([&results](std::string item) { results.push_back(item); });

    args = {"--vec", "one", "two", "three"};

    run();

    CHECK(dummy == results);
}

// #128
TEST_CASE_METHOD(TApp, "RepeatingMultiArgumentOptions", "[app]") {
    std::vector<std::string> entries;
    app.add_option("--entry", entries, "set a key and value")->type_name("KEY VALUE")->type_size(-2);

    args = {"--entry", "key1", "value1", "--entry", "key2", "value2"};
    REQUIRE_NOTHROW(run());
    CHECK(std::vector<std::string>({"key1", "value1", "key2", "value2"}) == entries);

    args.pop_back();
    REQUIRE_THROWS_AS(run(), CLI::ArgumentMismatch);
}

// #122
TEST_CASE_METHOD(TApp, "EmptyOptionEach", "[app]") {
    std::string q;
    app.add_option("--each")->each([&q](std::string s) { q = s; });

    args = {"--each", "that"};
    run();

    CHECK("that" == q);
}

// #122
TEST_CASE_METHOD(TApp, "EmptyOptionFail", "[app]") {
    std::string q;
    app.add_option("--each");

    args = {"--each", "that"};
    run();
}

TEST_CASE_METHOD(TApp, "BeforeRequirements", "[app]") {
    app.add_flag_function("-a", [](std::int64_t) { throw CLI::Success(); });
    app.add_flag_function("-b", [](std::int64_t) { throw CLI::CallForHelp(); });

    args = {"extra"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"-a", "extra"};
    CHECK_THROWS_AS(run(), CLI::Success);

    args = {"-b", "extra"};
    CHECK_THROWS_AS(run(), CLI::CallForHelp);

    // These run in definition order.
    args = {"-a", "-b", "extra"};
    CHECK_THROWS_AS(run(), CLI::Success);

    // Currently, the original order is not preserved when calling callbacks
    // args = {"-b", "-a", "extra"};
    // CHECK_THROWS_AS (run(), CLI::CallForHelp);
}

// #209
TEST_CASE_METHOD(TApp, "CustomUserSepParse", "[app]") {

    std::vector<int> vals{1, 2, 3};
    args = {"--idx", "1,2,3"};
    auto *opt = app.add_option("--idx", vals)->delimiter(',');
    run();
    CHECK(std::vector<int>({1, 2, 3}) == vals);
    std::vector<int> vals2;
    // check that the results vector gets the results in the same way
    opt->results(vals2);
    CHECK(vals == vals2);

    app.remove_option(opt);

    app.add_option("--idx", vals)->delimiter(',')->capture_default_str();
    run();
    CHECK(std::vector<int>({1, 2, 3}) == vals);
}

// #209
TEST_CASE_METHOD(TApp, "DefaultUserSepParse", "[app]") {

    std::vector<std::string> vals;
    args = {"--idx", "1 2 3", "4 5 6"};
    auto *opt = app.add_option("--idx", vals, "");
    run();
    CHECK(std::vector<std::string>({"1 2 3", "4 5 6"}) == vals);
    opt->delimiter(',');
    run();
    CHECK(std::vector<std::string>({"1 2 3", "4 5 6"}) == vals);
}

// #209
TEST_CASE_METHOD(TApp, "BadUserSepParse", "[app]") {

    std::vector<int> vals;
    app.add_option("--idx", vals);

    args = {"--idx", "1,2,3"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

// #209
TEST_CASE_METHOD(TApp, "CustomUserSepParse2", "[app]") {

    std::vector<int> vals{1, 2, 3};
    args = {"--idx", "1,2,"};
    auto *opt = app.add_option("--idx", vals)->delimiter(',');
    run();
    CHECK(std::vector<int>({1, 2}) == vals);

    app.remove_option(opt);

    app.add_option("--idx", vals, "")->delimiter(',')->capture_default_str();
    run();
    CHECK(std::vector<int>({1, 2}) == vals);
}

TEST_CASE_METHOD(TApp, "CustomUserSepParseFunction", "[app]") {

    std::vector<int> vals{1, 2, 3};
    args = {"--idx", "1,2,3"};
    app.add_option_function<std::vector<int>>("--idx", [&vals](std::vector<int> v) { vals = std::move(v); })
        ->delimiter(',');
    run();
    CHECK(std::vector<int>({1, 2, 3}) == vals);
}

// delimiter removal
TEST_CASE_METHOD(TApp, "CustomUserSepParseToggle", "[app]") {

    std::vector<std::string> vals;
    args = {"--idx", "1,2,3"};
    auto *opt = app.add_option("--idx", vals)->delimiter(',');
    run();
    CHECK(std::vector<std::string>({"1", "2", "3"}) == vals);
    opt->delimiter('\0');
    run();
    CHECK(std::vector<std::string>({"1,2,3"}) == vals);
    opt->delimiter(',');
    run();
    CHECK(std::vector<std::string>({"1", "2", "3"}) == vals);
}

// #209
TEST_CASE_METHOD(TApp, "CustomUserSepParse3", "[app]") {

    std::vector<int> vals = {1, 2, 3};
    args = {"--idx",
            "1",
            ","
            "2"};
    auto *opt = app.add_option("--idx", vals)->delimiter(',');
    run();
    CHECK(std::vector<int>({1, 2}) == vals);
    app.remove_option(opt);

    app.add_option("--idx", vals)->delimiter(',');
    run();
    CHECK(std::vector<int>({1, 2}) == vals);
}

// #209
TEST_CASE_METHOD(TApp, "CustomUserSepParse4", "[app]") {

    std::vector<int> vals;
    args = {"--idx", "1,    2"};
    auto *opt = app.add_option("--idx", vals)->delimiter(',')->capture_default_str();
    run();
    CHECK(std::vector<int>({1, 2}) == vals);

    app.remove_option(opt);

    app.add_option("--idx", vals)->delimiter(',');
    run();
    CHECK(std::vector<int>({1, 2}) == vals);
}

// #218
TEST_CASE_METHOD(TApp, "CustomUserSepParse5", "[app]") {

    std::vector<std::string> bar;
    args = {"this", "is", "a", "test"};
    auto *opt = app.add_option("bar", bar, "bar");
    run();
    CHECK(std::vector<std::string>({"this", "is", "a", "test"}) == bar);

    app.remove_option(opt);
    args = {"this", "is", "a", "test"};
    app.add_option("bar", bar, "bar")->capture_default_str();
    run();
    CHECK(std::vector<std::string>({"this", "is", "a", "test"}) == bar);
}

// #218
TEST_CASE_METHOD(TApp, "logFormSingleDash", "[app]") {
    bool verbose{false};
    bool veryverbose{false};
    bool veryveryverbose{false};
    app.name("testargs");
    app.allow_extras();
    args = {"-v", "-vv", "-vvv"};
    app.final_callback([&]() {
        auto rem = app.remaining();
        for(auto &arg : rem) {
            if(arg == "-v") {
                verbose = true;
            }
            if(arg == "-vv") {
                veryverbose = true;
            }
            if(arg == "-vvv") {
                veryveryverbose = true;
            }
        }
    });
    run();
    CHECK(app.remaining().size() == 3U);
    CHECK(verbose);
    CHECK(veryverbose);
    CHECK(veryveryverbose);
}

TEST_CASE("C20_compile", "simple") {
    CLI::App app{"test"};
    auto *flag = app.add_flag("--flag", "desc");

    app.parse("--flag");
    CHECK_FALSE(flag->empty());
}

// #845
TEST_CASE("Ensure UTF-8", "[app]") {
    const char *commandline = CLI11_ENSURE_UTF8_EXE " 1234 false \"hello world\"";
    int retval = std::system(commandline);

    if(retval == -1) {
        FAIL("Executable '" << commandline << "' reported that argv pointer changed where it should not have been");
    }

    if(retval > 0) {
        FAIL("Executable '" << commandline << "' reported different argv at index " << (retval - 1));
    }

    if(retval != 0) {
        FAIL("Executable '" << commandline << "' failed with an unknown return code");
    }
}

// #845
TEST_CASE("Ensure UTF-8 called twice", "[app]") {
    const char *commandline = CLI11_ENSURE_UTF8_TWICE_EXE " 1234 false \"hello world\"";
    int retval = std::system(commandline);

    if(retval == -1) {
        FAIL("Executable '" << commandline << "' reported that argv pointer changed where it should not have been");
    }

    if(retval > 0) {
        FAIL("Executable '" << commandline << "' reported different argv at index " << (retval - 1));
    }

    if(retval != 0) {
        FAIL("Executable '" << commandline << "' failed with an unknown return code");
    }
}
