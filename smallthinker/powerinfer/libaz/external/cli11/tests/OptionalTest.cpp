// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "app_helper.hpp"

// You can explicitly enable or disable support
// by defining to 1 or 0. Extra check here to ensure it's in the stdlib too.
// We nest the check for __has_include and it's usage
#ifndef CLI11_STD_OPTIONAL
#ifdef __has_include
#if defined(CLI11_CPP17) && __has_include(<optional>)
#define CLI11_STD_OPTIONAL 1
#else
#define CLI11_STD_OPTIONAL 0
#endif
#else
#define CLI11_STD_OPTIONAL 0
#endif
#endif

#ifndef CLI11_EXPERIMENTAL_OPTIONAL
#define CLI11_EXPERIMENTAL_OPTIONAL 0
#endif

#ifndef CLI11_BOOST_OPTIONAL
#define CLI11_BOOST_OPTIONAL 0
#endif

#if CLI11_BOOST_OPTIONAL
#include <boost/version.hpp>
#if BOOST_VERSION < 106100
#error "This boost::optional version is not supported, use 1.61 or better"
#endif
#endif

#if CLI11_STD_OPTIONAL
#include <optional>
#endif
#if CLI11_EXPERIMENTAL_OPTIONAL
#include <experimental/optional>
#endif
#if CLI11_BOOST_OPTIONAL
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#endif
// [CLI11:verbatim]

TEST_CASE("OptionalNoEmpty") { CHECK(1 == 1); }

#if CLI11_STD_OPTIONAL

#ifdef _MSC_VER
// this warning suppresses double to int conversions that are inherent in the test
// on windows.  This may be able to removed in the future as the add_option capability
// improves
#pragma warning(disable : 4244)
#endif

TEST_CASE_METHOD(TApp, "StdOptionalTest", "[optional]") {
    std::optional<int> opt;
    app.add_option("-c,--count", opt);
    run();
    CHECK(!opt);

    args = {"-c", "1"};
    run();
    CHECK((opt && (1 == *opt)));

    args = {"--count", "3"};
    run();
    CHECK((opt && (3 == *opt)));
}

TEST_CASE_METHOD(TApp, "StdOptionalVectorEmptyDirect", "[optional]") {
    std::optional<std::vector<int>> opt;
    app.add_option("-v,--vec", opt)->expected(0, 3)->allow_extra_args();
    // app.add_option("-v,--vec", opt)->expected(0, 3)->allow_extra_args();
    run();
    CHECK(!opt);
    args = {"-v"};
    opt = std::vector<int>{4, 3};
    run();
    CHECK(!opt);
    args = {"-v", "1", "4", "5"};
    run();
    REQUIRE(opt);
    std::vector<int> expV{1, 4, 5};
    CHECK(expV == *opt);
}

TEST_CASE_METHOD(TApp, "StdOptionalComplexDirect", "[optional]") {
    std::optional<std::complex<double>> opt;
    app.add_option("-c,--complex", opt)->type_size(0, 2);
    run();
    CHECK(!opt);
    args = {"-c"};
    opt = std::complex<double>{4.0, 3.0};
    run();
    CHECK(!opt);
    args = {"-c", "1+2j"};
    run();
    CHECK(opt);
    std::complex<double> val{1, 2};
    CHECK(val == *opt);
    args = {"-c", "3", "-4"};
    run();
    CHECK(opt);
    std::complex<double> val2{3, -4};
    CHECK(val2 == *opt);
}

TEST_CASE_METHOD(TApp, "StdOptionalUint", "[optional]") {
    std::optional<std::uint64_t> opt;
    app.add_option("-i,--int", opt);
    run();
    CHECK(!opt);

    args = {"-i", "15"};
    run();
    CHECK((opt && (15U == *opt)));
    static_assert(CLI::detail::classify_object<std::optional<std::uint64_t>>::value ==
                  CLI::detail::object_category::wrapper_value);
}

TEST_CASE_METHOD(TApp, "StdOptionalbool", "[optional]") {
    std::optional<bool> opt{};
    CHECK(!opt);
    app.add_flag("--opt,!--no-opt", opt);
    CHECK(!opt);
    run();
    CHECK(!opt);

    args = {"--opt"};
    run();
    CHECK((opt && *opt));

    args = {"--no-opt"};
    run();
    REQUIRE(opt);
    if(opt) {
        CHECK_FALSE(*opt);
    }
    static_assert(CLI::detail::classify_object<std::optional<bool>>::value ==
                  CLI::detail::object_category::wrapper_value);
}

#ifdef _MSC_VER
#pragma warning(default : 4244)
#endif

#endif
#if CLI11_EXPERIMENTAL_OPTIONAL

TEST_CASE_METHOD(TApp, "ExperimentalOptionalTest", "[optional]") {
    std::experimental::optional<int> opt;
    app.add_option("-c,--count", opt);
    run();
    CHECK(!opt);

    args = {"-c", "1"};
    run();
    CHECK(opt);
    CHECK(1 == *opt);

    args = {"--count", "3"};
    run();
    CHECK(opt);
    CHECK(3 == *opt);
}

#endif
#if CLI11_BOOST_OPTIONAL

TEST_CASE_METHOD(TApp, "BoostOptionalTest", "[optional]") {
    boost::optional<int> opt;
    app.add_option("-c,--count", opt);
    run();
    CHECK(!opt);

    args = {"-c", "1"};
    run();
    REQUIRE(opt);
    CHECK(1 == *opt);
    opt = {};
    args = {"--count", "3"};
    run();
    REQUIRE(opt);
    CHECK(3 == *opt);
}

TEST_CASE_METHOD(TApp, "BoostOptionalTestZarg", "[optional]") {
    boost::optional<int> opt;
    app.add_option("-c,--count", opt)->expected(0, 1);
    run();
    CHECK(!opt);

    args = {"-c", "1"};
    run();
    REQUIRE(opt);
    CHECK(1 == *opt);
    opt = {};
    args = {"--count"};
    run();
    CHECK(!opt);
}

TEST_CASE_METHOD(TApp, "BoostOptionalint64Test", "[optional]") {
    boost::optional<std::int64_t> opt;
    app.add_option("-c,--count", opt);
    run();
    CHECK(!opt);

    args = {"-c", "1"};
    run();
    REQUIRE(opt);
    CHECK(1 == *opt);
    opt = {};
    args = {"--count", "3"};
    run();
    REQUIRE(opt);
    CHECK(3 == *opt);
}

TEST_CASE_METHOD(TApp, "BoostOptionalStringTest", "[optional]") {
    boost::optional<std::string> opt;
    app.add_option("-s,--string", opt);
    run();
    CHECK(!opt);

    args = {"-s", "strval"};
    run();
    REQUIRE(opt);
    CHECK("strval" == *opt);
    opt = {};
    args = {"--string", "strv"};
    run();
    REQUIRE(opt);
    CHECK("strv" == *opt);
}
namespace boost {
using CLI::enums::operator<<;
}

TEST_CASE_METHOD(TApp, "BoostOptionalEnumTest", "[optional]") {

    enum class eval : char { val0 = 0, val1 = 1, val2 = 2, val3 = 3, val4 = 4 };
    boost::optional<eval> opt, opt2;

    auto optptr = app.add_option<decltype(opt), eval>("-v,--val", opt);
    app.add_option_no_stream("-e,--eval", opt2);
    optptr->capture_default_str();

    auto dstring = optptr->get_default_str();
    CHECK(dstring.empty());
    run();
    auto checkOpt = static_cast<bool>(opt);
    CHECK_FALSE(checkOpt);

    args = {"-v", "3"};
    run();
    checkOpt = static_cast<bool>(opt);
    REQUIRE(checkOpt);
    CHECK(*opt == eval::val3);
    opt = {};
    args = {"--val", "1"};
    run();
    checkOpt = static_cast<bool>(opt);
    REQUIRE(checkOpt);
    CHECK(*opt == eval::val1);
}

TEST_CASE_METHOD(TApp, "BoostOptionalVector", "[optional]") {
    boost::optional<std::vector<int>> opt;
    app.add_option_function<std::vector<int>>(
           "-v,--vec", [&opt](const std::vector<int> &v) { opt = v; }, "some vector")
        ->expected(3);
    run();
    bool checkOpt = static_cast<bool>(opt);
    CHECK(!checkOpt);

    args = {"-v", "1", "4", "5"};
    run();
    checkOpt = static_cast<bool>(opt);
    REQUIRE(checkOpt);
    std::vector<int> expV{1, 4, 5};
    CHECK(expV == *opt);
}

TEST_CASE_METHOD(TApp, "BoostOptionalVectorEmpty", "[optional]") {
    boost::optional<std::vector<int>> opt;
    app.add_option<decltype(opt), std::vector<int>>("-v,--vec", opt)->expected(0, 3)->allow_extra_args();
    // app.add_option("-v,--vec", opt)->expected(0, 3)->allow_extra_args();
    run();
    bool checkOpt = static_cast<bool>(opt);
    CHECK(!checkOpt);
    args = {"-v"};
    opt = std::vector<int>{4, 3};
    run();
    checkOpt = static_cast<bool>(opt);
    CHECK(!checkOpt);
    args = {"-v", "1", "4", "5"};
    run();
    checkOpt = static_cast<bool>(opt);
    REQUIRE(checkOpt);
    std::vector<int> expV{1, 4, 5};
    CHECK(expV == *opt);
}

TEST_CASE_METHOD(TApp, "BoostOptionalVectorEmptyDirect", "[optional]") {
    boost::optional<std::vector<int>> opt;
    app.add_option_no_stream("-v,--vec", opt)->expected(0, 3)->allow_extra_args();
    // app.add_option("-v,--vec", opt)->expected(0, 3)->allow_extra_args();
    run();
    bool checkOpt = static_cast<bool>(opt);
    CHECK(!checkOpt);
    args = {"-v"};
    opt = std::vector<int>{4, 3};
    run();
    checkOpt = static_cast<bool>(opt);
    CHECK(!checkOpt);
    args = {"-v", "1", "4", "5"};
    run();
    checkOpt = static_cast<bool>(opt);
    REQUIRE(checkOpt);
    std::vector<int> expV{1, 4, 5};
    CHECK(expV == *opt);
}

TEST_CASE_METHOD(TApp, "BoostOptionalComplexDirect", "[optional]") {
    boost::optional<std::complex<double>> opt;
    app.add_option("-c,--complex", opt)->type_size(0, 2);
    run();
    CHECK(!opt);
    args = {"-c"};
    opt = std::complex<double>{4.0, 3.0};
    run();
    CHECK(!opt);
    args = {"-c", "1+2j"};
    run();
    REQUIRE(opt);
    std::complex<double> val{1, 2};
    CHECK(val == *opt);
    args = {"-c", "3", "-4"};
    run();
    REQUIRE(opt);
    std::complex<double> val2{3, -4};
    CHECK(val2 == *opt);
}

#endif
