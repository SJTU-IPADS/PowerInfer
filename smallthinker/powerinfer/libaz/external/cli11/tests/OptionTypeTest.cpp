// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include "catch.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <forward_list>
#include <limits>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using Catch::literals::operator"" _a;

TEST_CASE_METHOD(TApp, "OneStringAgain", "[optiontype]") {
    std::string str;
    app.add_option("-s,--string", str);
    args = {"--string", "mystring"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "OneStringFunction", "[optiontype]") {
    std::string str;
    app.add_option_function<std::string>("-s,--string", [&str](const std::string &val) { str = val; });
    args = {"--string", "mystring"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--string") == 1u);
    CHECK("mystring" == str);
}

TEST_CASE_METHOD(TApp, "doubleFunction", "[optiontype]") {
    double res{0.0};
    app.add_option_function<double>("--val", [&res](double val) { res = std::abs(val + 54); });
    args = {"--val", "-354.356"};
    run();
    CHECK(300.356_a == res);
    // get the original value as entered as an integer
    CHECK(-354.356_a == app["--val"]->as<float>());
}

TEST_CASE_METHOD(TApp, "doubleFunctionFail", "[optiontype]") {
    double res = NAN;
    app.add_option_function<double>("--val", [&res](double val) { res = std::abs(val + 54); });
    args = {"--val", "not_double"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "doubleVectorFunction", "[optiontype]") {
    std::vector<double> res;
    app.add_option_function<std::vector<double>>("--val", [&res](const std::vector<double> &val) {
        res = val;
        std::transform(res.begin(), res.end(), res.begin(), [](double v) { return v + 5.0; });
    });
    args = {"--val", "5", "--val", "6", "--val", "7"};
    run();
    CHECK(3u == res.size());
    CHECK(10.0_a == res[0]);
    CHECK(12.0_a == res[2]);
}

TEST_CASE_METHOD(TApp, "doubleVectorFunctionFail", "[optiontype]") {
    std::vector<double> res;
    std::string vstring = "--val";
    app.add_option_function<std::vector<double>>(vstring, [&res](const std::vector<double> &val) {
        res = val;
        std::transform(res.begin(), res.end(), res.begin(), [](double v) { return v + 5.0; });
    });
    args = {"--val", "five", "--val", "nine", "--val", "7"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
    // check that getting the results through the results function generates the same error
    CHECK_THROWS_AS(app[vstring]->results(res), CLI::ConversionError);
    auto strvec = app[vstring]->as<std::vector<std::string>>();
    CHECK(3u == strvec.size());
}

TEST_CASE_METHOD(TApp, "doubleVectorFunctionRunCallbackOnDefault", "[optiontype]") {
    std::vector<double> res;
    auto *opt = app.add_option_function<std::vector<double>>("--val", [&res](const std::vector<double> &val) {
        res = val;
        std::transform(res.begin(), res.end(), res.begin(), [](double v) { return v + 5.0; });
    });
    args = {"--val", "5", "--val", "6", "--val", "7"};
    run();
    CHECK(3u == res.size());
    CHECK(10.0 == res[0]);
    CHECK(12.0 == res[2]);
    CHECK(!opt->get_run_callback_for_default());
    opt->run_callback_for_default();
    opt->default_val(std::vector<int>{2, 1, -2});
    CHECK(7.0 == res[0]);
    CHECK(3.0 == res[2]);

    CHECK_THROWS_AS(opt->default_val("this is a string"), CLI::ConversionError);
    auto vec = opt->as<std::vector<double>>();
    REQUIRE(3U == vec.size());
    CHECK(5.0 == vec[0]);
    CHECK(7.0 == vec[2]);
    opt->check(CLI::Number);
    opt->run_callback_for_default(false);
    CHECK_THROWS_AS(opt->default_val("this is a string"), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "BoolAndIntFlags", "[optiontype]") {

    bool bflag{false};
    int iflag{0};
    unsigned int uflag{0};

    app.add_flag("-b", bflag);
    app.add_flag("-i", iflag);
    app.add_flag("-u", uflag);

    args = {"-b", "-i", "-u"};
    run();
    CHECK(bflag);
    CHECK(iflag == 1);
    CHECK(uflag == (unsigned int)1);

    args = {"-b", "-b"};
    REQUIRE_NOTHROW(run());
    CHECK(bflag);

    bflag = false;

    args = {"-iiiuu"};
    run();
    CHECK(!bflag);
    CHECK(iflag == 3);
    CHECK(uflag == (unsigned int)2);
}

TEST_CASE_METHOD(TApp, "atomic_bool_flags", "[optiontype]") {

    std::atomic<bool> bflag{false};
    std::atomic<int> iflag{0};

    app.add_flag("-b", bflag);
    app.add_flag("-i,--int", iflag)->multi_option_policy(CLI::MultiOptionPolicy::Sum);

    args = {"-b", "-i"};
    run();
    CHECK(bflag.load());
    CHECK(iflag.load() == 1);

    args = {"-b", "-b"};
    REQUIRE_NOTHROW(run());
    CHECK(bflag.load());

    bflag = false;

    args = {"-iii"};
    run();
    CHECK(!bflag.load());
    CHECK(iflag.load() == 3);
    args = {"--int=notanumber"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "BoolOption", "[optiontype]") {
    bool bflag{false};
    app.add_option("-b", bflag);

    args = {"-b", "false"};
    run();
    CHECK(!bflag);

    args = {"-b", "1"};
    run();
    CHECK(bflag);

    args = {"-b", "-7"};
    run();
    CHECK(!bflag);

    // cause an out of bounds error internally
    args = {"-b", "751615654161688126132138844896646748852"};
    run();
    CHECK(bflag);

    args = {"-b", "-751615654161688126132138844896646748852"};
    run();
    CHECK(!bflag);
}

TEST_CASE_METHOD(TApp, "atomic_int_option", "[optiontype]") {
    std::atomic<int> i{0};
    auto *aopt = app.add_option("-i,--int", i);
    args = {"-i4"};
    run();
    CHECK(app.count("--int") == 1u);
    CHECK(app.count("-i") == 1u);
    CHECK(4 == i);
    CHECK("4" == app["-i"]->as<std::string>());
    CHECK(4.0 == app["--int"]->as<double>());

    args = {"--int", "notAnInt"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    aopt->expected(0, 1);
    args = {"--int"};
    run();
    CHECK(0 == i);
}

static const std::map<std::string, double> testValuesDouble{
    {"3.14159", 3.14159},
    {"-3.14159", -3.14159},
    {"-3.14159\t", -3.14159},
    {"-3.14159  ", -3.14159},
    {"+1.0", 1.0},
    {"-0.01", -0.01},
    {"5e22", 5e22},
    {" 5e22", 5e22},
    {" 5e22  ", 5e22},
    {"-2E-2", -2e-2},
    {"5e+22", 5e22},
    {"1e06", 1e6},
    {"6.626e-34", 6.626e-34},
    {"6.626e+34", 6.626e34},
    {"-6.626e-34", -6.626e-34},
    {"224_617.445_991", 224617.445991},
    {"224'617.445'991", 224617.445991},
    {"inf", std::numeric_limits<double>::infinity()},
    {"+inf", std::numeric_limits<double>::infinity()},
    {"-inf", -std::numeric_limits<double>::infinity()},
    {"nan", std::numeric_limits<double>::signaling_NaN()},
    {"+nan", std::numeric_limits<double>::signaling_NaN()},
    {"-nan", -std::numeric_limits<double>::signaling_NaN()},

};

TEST_CASE_METHOD(TApp, "floatingConversions", "[optiontype]") {
    auto test_data = GENERATE(from_range(testValuesDouble));

    double val{0};
    app.add_option("--val", val);

    args = {"--val", test_data.first};

    run();
    if(std::isnan(test_data.second)) {
        CHECK(std::isnan(val));
    } else {

        CHECK_THAT(val, WithinRel(test_data.second, 1e-11));
    }
}

static const std::map<std::string, std::int64_t> testValuesInt{
    {"+99", 99},
    {"99", 99},
    {"-99", -99},
    {"-99 ", -99},
    {"0xDEADBEEF", 0xDEADBEEF},
    {"0xdeadbeef", 0xDEADBEEF},
    {"0XDEADBEEF", 0xDEADBEEF},
    {"0Xdeadbeef", 0xDEADBEEF},
    {"0xdead_beef", 0xDEADBEEF},
    {"0xdead'beef", 0xDEADBEEF},
    {"0o01234567", 001234567},
    {"0o755", 0755},
    {"0755", 0755},
    {"995862_262", 995862262},
    {"995862262", 995862262},
    {"-995862275", -995862275},
    {"\t-995862275\t", -995862275},
    {"-995'862'275", -995862275},
    {"0b11010110", 0xD6},
    {"0b1101'0110", 0xD6},
    {"0B11010110", 0xD6},
    {"0B1101'0110", 0xD6},
    {"1_2_3_4_5", 12345},
};

TEST_CASE_METHOD(TApp, "intConversions", "[optiontype]") {

    auto test_data = GENERATE(from_range(testValuesInt));

    std::int64_t val{0};
    app.add_option("--val", val);

    args = {"--val", test_data.first};

    run();

    CHECK(val == test_data.second);
}

TEST_CASE_METHOD(TApp, "intConversionsErange", "[optiontype]") {

    std::int64_t val{0};
    app.add_option("--val", val);

    args = {"--val", "0o11545241241415151512312415123125667"};

    CHECK_THROWS_AS(run(), CLI::ParseError);

    args = {"--val", "0b1011000001101011001100110011111000101010101011111111111111111111111001010111011100"};

    CHECK_THROWS_AS(run(), CLI::ParseError);

    args = {"--val", "0B1011000001101011001100110011111000101010101011111111111111111111111001010111011100"};

    CHECK_THROWS_AS(run(), CLI::ParseError);
}

static const std::map<std::string, std::uint64_t> testValuesUInt{
    {"+99", 99},
    {"99", 99},
    {" 99 ", 99},
    {"0xDEADBEEF", 0xDEADBEEF},
    {"0xdeadbeef", 0xDEADBEEF},
    {"0XDEADBEEF", 0xDEADBEEF},
    {"0Xdeadbeef", 0xDEADBEEF},
    {"0xdead_beef", 0xDEADBEEF},
    {"0xdead'beef", 0xDEADBEEF},
    {"0o01234567", 001234567},
    {"0o755", 0755},
    {"0o755\t", 0755},
    {"0755", 0755},
    {"995862_262", 995862262},
    {"995862262", 995862262},
    {"+995862275", +995862275},
    {"+995862275         \n\t", +995862275},
    {"995'862'275", 995862275},
    {"0b11010110", 0xD6},
    {"0b1101'0110", 0xD6},
    {"0b1101'0110                                                       ", 0xD6},
    {"0B11010110", 0xD6},
    {"0B1101'0110", 0xD6},
    {"1_2_3_4_5", 12345},
};

TEST_CASE_METHOD(TApp, "uintConversions", "[optiontype]") {

    auto test_data = GENERATE(from_range(testValuesUInt));

    std::uint64_t val{0};
    app.add_option("--val", val);

    args = {"--val", test_data.first};

    run();

    CHECK(val == test_data.second);
}

TEST_CASE_METHOD(TApp, "uintConversionsErange", "[optiontype]") {

    std::uint64_t val{0};
    app.add_option("--val", val);

    args = {"--val", "0o11545241241415151512312415123125667"};

    CHECK_THROWS_AS(run(), CLI::ParseError);

    args = {"--val", "0b1011000001101011001100110011111000101010101011111111111111111111111001010111011100"};

    CHECK_THROWS_AS(run(), CLI::ParseError);

    args = {"--val", "0B1011000001101011001100110011111000101010101011111111111111111111111001010111011100"};

    CHECK_THROWS_AS(run(), CLI::ParseError);
}

TEST_CASE_METHOD(TApp, "CharOption", "[optiontype]") {
    char c1{'t'};
    app.add_option("-c", c1);

    args = {"-c", "g"};
    run();
    CHECK('g' == c1);

    args = {"-c", "1"};
    run();
    CHECK('1' == c1);

    args = {"-c", "77"};
    run();
    CHECK(77 == c1);

    // convert hex for digit
    args = {"-c", "0x44"};
    run();
    CHECK(0x44 == c1);

    args = {"-c", "751615654161688126132138844896646748852"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "vectorDefaults", "[optiontype]") {
    std::vector<int> vals{4, 5};
    auto *opt = app.add_option("--long", vals)->capture_default_str();

    args = {"--long", "[1,2,3]"};

    run();

    CHECK(std::vector<int>({1, 2, 3}) == vals);

    args.clear();
    run();
    auto res = app["--long"]->as<std::vector<int>>();
    CHECK(std::vector<int>({4, 5}) == res);

    app.clear();
    opt->expected(1)->take_last();
    res = app["--long"]->as<std::vector<int>>();
    CHECK(std::vector<int>({5}) == res);
    opt->take_first();
    res = app["--long"]->as<std::vector<int>>();
    CHECK(std::vector<int>({4}) == res);

    opt->expected(0, 1)->take_last();
    run();

    CHECK(std::vector<int>({4}) == res);
    res = app["--long"]->as<std::vector<int>>();
    CHECK(std::vector<int>({5}) == res);
}

TEST_CASE_METHOD(TApp, "mapInput", "[optiontype]") {
    std::map<int, std::string> vals{};
    app.add_option("--long", vals);

    args = {"--long", "5", "test"};

    run();

    CHECK(vals.at(5) == "test");
}

TEST_CASE_METHOD(TApp, "CallbackBoolFlags", "[optiontype]") {

    bool value{false};

    auto func = [&value]() { value = true; };

    auto *cback = app.add_flag_callback("--val", func);
    args = {"--val"};
    run();
    CHECK(value);
    value = false;
    args = {"--val=false"};
    run();
    CHECK(!value);

    CHECK_THROWS_AS(app.add_flag_callback("hi", func), CLI::IncorrectConstruction);
    cback->multi_option_policy(CLI::MultiOptionPolicy::Throw);
    args = {"--val", "--val=false"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "pair_check", "[optiontype]") {
    std::string myfile{"pair_check_file.txt"};
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    CHECK(ok);

    CHECK(CLI::ExistingFile(myfile).empty());
    std::pair<std::string, int> findex;

    auto v0 = CLI::ExistingFile;
    v0.application_index(0);
    auto v1 = CLI::PositiveNumber;
    v1.application_index(1);
    app.add_option("--file", findex)->check(v0)->check(v1);

    args = {"--file", myfile, "2"};

    CHECK_NOTHROW(run());

    CHECK(myfile == findex.first);
    CHECK(2 == findex.second);

    args = {"--file", myfile, "-3"};

    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--file", myfile, "2"};
    std::remove(myfile.c_str());
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "pair_check_string", "[optiontype]") {
    std::string myfile{"pair_check_file.txt"};
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    CHECK(ok);

    CHECK(CLI::ExistingFile(myfile).empty());
    std::pair<std::string, std::string> findex;

    auto v0 = CLI::ExistingFile;
    v0.application_index(0);
    auto v1 = CLI::PositiveNumber;
    v1.application_index(1);
    app.add_option("--file", findex)->check(v0)->check(v1);

    args = {"--file", myfile, "2"};

    CHECK_NOTHROW(run());

    CHECK(myfile == findex.first);
    CHECK("2" == findex.second);

    args = {"--file", myfile, "-3"};

    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--file", myfile, "2"};
    std::remove(myfile.c_str());
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "pair_check_take_first", "[optiontype]") {
    std::string myfile{"pair_check_file2.txt"};
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    CHECK(ok);

    CHECK(CLI::ExistingFile(myfile).empty());
    std::pair<std::string, int> findex;

    auto *opt = app.add_option("--file", findex)->check(CLI::ExistingFile)->check(CLI::PositiveNumber);
    CHECK_THROWS_AS(opt->get_validator(3), CLI::OptionNotFound);
    opt->get_validator(0)->application_index(0);
    opt->get_validator(1)->application_index(1);
    opt->multi_option_policy(CLI::MultiOptionPolicy::TakeLast);
    args = {"--file", "not_a_file.txt", "-16", "--file", myfile, "2"};
    // should only check the last one
    CHECK_NOTHROW(run());

    CHECK(myfile == findex.first);
    CHECK(2 == findex.second);

    opt->multi_option_policy(CLI::MultiOptionPolicy::TakeFirst);

    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "VectorFixedString", "[optiontype]") {
    std::vector<std::string> strvec;
    std::vector<std::string> answer{"mystring", "mystring2", "mystring3"};

    CLI::Option *opt = app.add_option("-s,--string", strvec)->expected(3);
    CHECK(opt->get_expected() == 3);

    args = {"--string", "mystring", "mystring2", "mystring3"};
    run();
    CHECK(app.count("--string") == 3u);
    CHECK(strvec == answer);
}

TEST_CASE_METHOD(TApp, "VectorDefaultedFixedString", "[optiontype]") {
    std::vector<std::string> strvec{"one"};
    std::vector<std::string> answer{"mystring", "mystring2", "mystring3"};

    CLI::Option *opt = app.add_option("-s,--string", strvec, "")->expected(3)->capture_default_str();
    CHECK(opt->get_expected() == 3);

    args = {"--string", "mystring", "mystring2", "mystring3"};
    run();
    CHECK(app.count("--string") == 3u);
    CHECK(strvec == answer);
}

TEST_CASE_METHOD(TApp, "VectorIndexedValidator", "[optiontype]") {
    std::vector<int> vvec;

    CLI::Option *opt = app.add_option("-v", vvec);

    args = {"-v", "1", "-1", "-v", "3", "-v", "-976"};
    run();
    CHECK(app.count("-v") == 4u);
    CHECK(vvec.size() == 4u);
    opt->check(CLI::PositiveNumber.application_index(0));
    opt->check((!CLI::PositiveNumber).application_index(1));
    CHECK_NOTHROW(run());
    CHECK(vvec.size() == 4u);
    // v[3] would be negative
    opt->check(CLI::PositiveNumber.application_index(3));
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "IntegerOverFlowShort", "[optiontype]") {
    std::int16_t A{0};
    std::uint16_t B{0};

    app.add_option("-a", A);
    app.add_option("-b", B);

    args = {"-a", "2626254242"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "2626254242"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-26262"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-262624262525"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "IntegerOverFlowInt", "[optiontype]") {
    int A{0};
    unsigned int B{0};

    app.add_option("-a", A);
    app.add_option("-b", B);

    args = {"-a", "262625424225252"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "262625424225252"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-2626225252"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-26262426252525252"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "IntegerOverFlowLong", "[optiontype]") {
    std::int32_t A{0};
    std::uint32_t B{0};

    app.add_option("-a", A);
    app.add_option("-b", B);

    args = {"-a", "1111111111111111111111111111"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "1111111111111111111111111111"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-2626225252"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-111111111111111111111111"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "IntegerOverFlowLongLong", "[optiontype]") {
    std::int64_t A{0};
    std::uint64_t B{0};

    app.add_option("-a", A);
    app.add_option("-b", B);

    args = {"-a", "1111111111111111111111111111111111111111111111111111111111"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "1111111111111111111111111111111111111111111111111111111111"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-2626225252"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-b", "-111111111111111111111111111111111111111111111111111111111"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "VectorUnlimString", "[optiontype]") {
    std::vector<std::string> strvec;
    std::vector<std::string> answer{"mystring", "mystring2", "mystring3"};

    CLI::Option *opt = app.add_option("-s,--string", strvec);
    CHECK(opt->get_expected() == 1);
    CHECK(opt->get_expected_max() == CLI::detail::expected_max_vector_size);

    args = {"--string", "mystring", "mystring2", "mystring3"};
    run();
    CHECK(app.count("--string") == 3u);
    CHECK(strvec == answer);

    args = {"-s", "mystring", "mystring2", "mystring3"};
    run();
    CHECK(app.count("--string") == 3u);
    CHECK(strvec == answer);
}

// From https://github.com/CLIUtils/CLI11/issues/420
TEST_CASE_METHOD(TApp, "stringLikeTests", "[optiontype]") {
    struct nType {
        explicit nType(std::string a_value) : m_value{std::move(a_value)} {}

        explicit operator std::string() const { return std::string{"op str"}; }

        std::string m_value;
    };

    nType m_type{"abc"};
    app.add_option("--type", m_type, "type")->capture_default_str();
    run();

    CHECK("op str" == app["--type"]->as<std::string>());
    args = {"--type", "bca"};
    run();
    CHECK("op str" == std::string(m_type));
    CHECK("bca" == m_type.m_value);
}

TEST_CASE_METHOD(TApp, "VectorExpectedRange", "[optiontype]") {
    std::vector<std::string> strvec;

    CLI::Option *opt = app.add_option("--string", strvec);
    opt->expected(2, 4)->multi_option_policy(CLI::MultiOptionPolicy::Throw);

    args = {"--string", "mystring", "mystring2", "mystring3"};
    run();
    CHECK(app.count("--string") == 3u);

    args = {"--string", "mystring"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--string", "mystring", "mystring2", "string2", "--string", "string4", "string5"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    CHECK(4 == opt->get_expected_max());
    CHECK(2 == opt->get_expected_min());
    opt->expected(4, 2);  // just test the handling of reversed arguments
    CHECK(4 == opt->get_expected_max());
    CHECK(2 == opt->get_expected_min());
    opt->expected(-5);
    CHECK(5 == opt->get_expected_max());
    CHECK(5 == opt->get_expected_min());
    opt->expected(-5, 7);
    CHECK(7 == opt->get_expected_max());
    CHECK(5 == opt->get_expected_min());
}

TEST_CASE_METHOD(TApp, "VectorFancyOpts", "[optiontype]") {
    std::vector<std::string> strvec;
    std::vector<std::string> answer{"mystring", "mystring2", "mystring3"};

    CLI::Option *opt = app.add_option("-s,--string", strvec)->required()->expected(3);
    CHECK(opt->get_expected() == 3);

    args = {"--string", "mystring", "mystring2", "mystring3"};
    run();
    CHECK(app.count("--string") == 3u);
    CHECK(strvec == answer);

    args = {"one", "two"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    CHECK_THROWS_AS(run(), CLI::ParseError);
}

// #87
TEST_CASE_METHOD(TApp, "CustomDoubleOption", "[optiontype]") {

    std::pair<int, double> custom_opt;

    auto *opt = app.add_option("posit", [&custom_opt](CLI::results_t vals) {
        custom_opt = {stol(vals.at(0)), stod(vals.at(1))};
        return true;
    });
    opt->type_name("INT FLOAT")->type_size(2);

    args = {"12", "1.5"};

    run();
    CHECK(12 == custom_opt.first);
    CHECK(1.5 == Approx(custom_opt.second));
}

// now with tuple support this is possible
TEST_CASE_METHOD(TApp, "CustomDoubleOptionAlt", "[optiontype]") {

    std::pair<int, double> custom_opt;

    app.add_option("posit", custom_opt);

    args = {"12", "1.5"};

    run();
    CHECK(12 == custom_opt.first);
    CHECK(1.5 == Approx(custom_opt.second));
}

// now with tuple support this is possible
TEST_CASE_METHOD(TApp, "floatPair", "[optiontype]") {

    std::pair<float, float> custom_opt;

    auto *opt = app.add_option("--fp", custom_opt)->delimiter(',');
    opt->default_str("3.4,2.7");

    args = {"--fp", "12", "1.5"};

    run();
    CHECK(12.0f == Approx(custom_opt.first));
    CHECK(1.5f == Approx(custom_opt.second));
    args = {};
    opt->force_callback();
    run();
    CHECK(3.4f == Approx(custom_opt.first));
    CHECK(2.7f == Approx(custom_opt.second));
}

// now with independent type sizes and expected this is possible
TEST_CASE_METHOD(TApp, "vectorPair", "[optiontype]") {

    std::vector<std::pair<int, std::string>> custom_opt;

    auto *opt = app.add_option("--dict", custom_opt);

    args = {"--dict", "1", "str1", "--dict", "3", "str3"};

    run();
    REQUIRE(2u == custom_opt.size());
    CHECK(1 == custom_opt[0].first);
    CHECK("str3" == custom_opt[1].second);

    args = {"--dict", "1", "str1", "--dict", "3", "str3", "--dict", "-1", "str4"};
    run();
    REQUIRE(3u == custom_opt.size());
    CHECK(-1 == custom_opt[2].first);
    CHECK("str4" == custom_opt[2].second);
    opt->check(CLI::PositiveNumber.application_index(0));

    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "vectorPairFail", "[optiontype]") {

    std::vector<std::pair<int, std::string>> custom_opt;

    app.add_option("--dict", custom_opt);

    args = {"--dict", "1", "str1", "--dict", "str3", "1"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "vectorPairFail2", "[optiontype]") {

    std::vector<std::pair<int, int>> custom_opt;

    auto *opt = app.add_option("--pairs", custom_opt);

    args = {"--pairs", "1", "2", "3", "4"};

    run();
    CHECK(custom_opt.size() == 2U);

    args = {"--pairs", "1", "2", "3"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
    // now change the type size to explicitly allow 1 or 2
    opt->type_size(1, 2);

    run();
    CHECK(custom_opt.size() == 2U);
}

TEST_CASE_METHOD(TApp, "vectorPairTypeRange", "[optiontype]") {

    std::vector<std::pair<int, std::string>> custom_opt;

    auto *opt = app.add_option("--dict", custom_opt);

    opt->type_size(2, 1);  // just test switched arguments
    CHECK(1 == opt->get_type_size_min());
    CHECK(2 == opt->get_type_size_max());

    args = {"--dict", "1", "str1", "--dict", "3", "str3"};

    run();
    REQUIRE(2u == custom_opt.size());
    CHECK(1 == custom_opt[0].first);
    CHECK("str3" == custom_opt[1].second);

    args = {"--dict", "1", "str1", "--dict", "3", "--dict", "-1", "str4"};
    run();
    REQUIRE(3u == custom_opt.size());
    CHECK(custom_opt[1].second.empty());
    CHECK(-1 == custom_opt[2].first);
    CHECK("str4" == custom_opt[2].second);

    opt->type_size(-2, -1);  // test negative arguments
    CHECK(1 == opt->get_type_size_min());
    CHECK(2 == opt->get_type_size_max());
    // this type size spec should run exactly as before
    run();
    REQUIRE(3u == custom_opt.size());
    CHECK(custom_opt[1].second.empty());
    CHECK(-1 == custom_opt[2].first);
    CHECK("str4" == custom_opt[2].second);
}

// now with independent type sizes and expected this is possible
TEST_CASE_METHOD(TApp, "vectorTuple", "[optiontype]") {

    std::vector<std::tuple<int, std::string, double>> custom_opt;

    auto *opt = app.add_option("--dict", custom_opt);

    args = {"--dict", "1", "str1", "4.3", "--dict", "3", "str3", "2.7"};

    run();
    REQUIRE(2u == custom_opt.size());
    CHECK(1 == std::get<0>(custom_opt[0]));
    CHECK("str3" == std::get<1>(custom_opt[1]));
    CHECK(2.7 == std::get<2>(custom_opt[1]));

    args = {"--dict", "1", "str1", "4.3", "--dict", "3", "str3", "2.7", "--dict", "-1", "str4", "-1.87"};
    run();
    REQUIRE(3u == custom_opt.size());
    CHECK(-1 == std::get<0>(custom_opt[2]));
    CHECK("str4" == std::get<1>(custom_opt[2]));
    CHECK(-1.87 == std::get<2>(custom_opt[2]));
    opt->check(CLI::PositiveNumber.application_index(0));

    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args.back() = "haha";
    args[9] = "45";
    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

// now with independent type sizes and expected this is possible
TEST_CASE_METHOD(TApp, "vectorVector", "[optiontype]") {

    std::vector<std::vector<int>> custom_opt;

    auto *opt = app.add_option("--dict", custom_opt);

    args = {"--dict", "1", "2", "4", "--dict", "3", "1"};

    run();
    REQUIRE(2u == custom_opt.size());
    CHECK(3u == custom_opt[0].size());
    CHECK(2u == custom_opt[1].size());

    args = {"--dict", "1", "2", "4", "--dict", "3", "1", "--dict", "3", "--dict",
            "3",      "3", "3", "3", "3",      "3", "3", "3",      "3", "-3"};
    run();
    REQUIRE(4u == custom_opt.size());
    CHECK(3u == custom_opt[0].size());
    CHECK(2u == custom_opt[1].size());
    CHECK(1u == custom_opt[2].size());
    CHECK(10u == custom_opt[3].size());
    opt->check(CLI::PositiveNumber.application_index(9));

    CHECK_THROWS_AS(run(), CLI::ValidationError);
    args.pop_back();
    CHECK_NOTHROW(run());

    args.back() = "haha";
    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"--dict", "1", "2", "4", "%%", "3", "1", "%%", "3", "%%", "3", "3", "3", "3", "3", "3", "3", "3", "3", "3"};
    run();
    REQUIRE(4u == custom_opt.size());
}

// now with independent type sizes and expected this is possible
TEST_CASE_METHOD(TApp, "vectorVectorFixedSize", "[optiontype]") {

    std::vector<std::vector<int>> custom_opt;

    auto *opt = app.add_option("--dict", custom_opt)->type_size(4);

    args = {"--dict", "1", "2", "4", "3", "--dict", "3", "1", "2", "8"};

    run();
    REQUIRE(2u == custom_opt.size());
    CHECK(4u == custom_opt[0].size());
    CHECK(4u == custom_opt[1].size());

    args = {"--dict", "1", "2", "4", "--dict", "3", "1", "7", "6"};
    CHECK_THROWS_AS(run(), CLI::ConversionError);
    // this should reset it
    opt->type_size(CLI::detail::expected_max_vector_size);
    opt->type_size(1, CLI::detail::expected_max_vector_size);
    CHECK_NOTHROW(run());
    REQUIRE(2U == custom_opt.size());
}

// now with independent type sizes and expected this is possible
TEST_CASE_METHOD(TApp, "tuplePair", "[optiontype]") {
    std::tuple<std::pair<int, double>> custom_opt;

    app.add_option("--pr", custom_opt);

    args = {"--pr", "1", "2"};

    run();
    CHECK(1 == std::get<0>(custom_opt).first);
    CHECK(2.0 == std::get<0>(custom_opt).second);
}
// now with independent type sizes and expected this is possible
TEST_CASE_METHOD(TApp, "tupleintPair", "[optiontype]") {
    std::tuple<int, std::pair<int, double>> custom_opt;

    app.add_option("--pr", custom_opt);

    args = {"--pr", "3", "1", "2"};

    run();
    CHECK(3 == std::get<0>(custom_opt));
    CHECK(1 == std::get<1>(custom_opt).first);
    CHECK(2.0 == std::get<1>(custom_opt).second);
}

static_assert(CLI::detail::is_mutable_container<std::set<std::string>>::value, "set should be a container");
static_assert(CLI::detail::is_mutable_container<std::map<std::string, std::string>>::value,
              "map should be a container");
static_assert(CLI::detail::is_mutable_container<std::unordered_map<std::string, double>>::value,
              "unordered_map should be a container");

static_assert(CLI::detail::is_mutable_container<std::list<std::pair<int, std::string>>>::value,
              "list should be a container");

static_assert(CLI::detail::type_count<std::set<std::string>>::value == 1, "set should have a type size of 1");
static_assert(CLI::detail::type_count<std::set<std::tuple<std::string, int, int>>>::value == 3,
              "tuple set should have size of 3");
static_assert(CLI::detail::type_count<std::map<std::string, std::string>>::value == 2,
              "map should have a type size of 2");
static_assert(CLI::detail::type_count<std::unordered_map<std::string, double>>::value == 2,
              "unordered_map should have a type size of 2");

static_assert(CLI::detail::type_count<std::list<std::pair<int, std::string>>>::value == 2,
              "list<int,string> should have a type size of 2");
static_assert(CLI::detail::type_count<std::map<std::string, std::pair<int, std::string>>>::value == 3,
              "map<string,pair<int,string>> should have a type size of 3");

TEMPLATE_TEST_CASE("Container int single",
                   "[optiontype]",
                   std::vector<int>,
                   std::deque<int>,
                   std::set<int>,
                   std::list<int>,
                   std::unordered_set<int>) {
    TApp tapp;
    TestType cv;

    CLI::Option *opt = tapp.app.add_option("-v", cv);

    tapp.args = {"-v", "1", "-1", "-v", "3", "-v", "-976"};
    tapp.run();
    CHECK(tapp.app.count("-v") == 4u);
    CHECK(cv.size() == 4u);
    opt->check(CLI::PositiveNumber.application_index(0));
    opt->check((!CLI::PositiveNumber).application_index(1));
    CHECK_NOTHROW(tapp.run());
    CHECK(cv.size() == 4u);
    // v[3] would be negative
    opt->check(CLI::PositiveNumber.application_index(3));
    CHECK_THROWS_AS(tapp.run(), CLI::ValidationError);
}

using isp = std::pair<int, std::string>;

TEMPLATE_TEST_CASE("Container pair",
                   "[optiontype]",
                   std::vector<isp>,
                   std::deque<isp>,
                   std::set<isp>,
                   std::list<isp>,
                   (std::map<int, std::string>),
                   (std::unordered_map<int, std::string>)) {
    TApp tapp;
    TestType cv;

    (tapp.app).add_option("--dict", cv);

    tapp.args = {"--dict", "1", "str1", "--dict", "3", "str3"};

    tapp.run();
    CHECK(2u == cv.size());

    tapp.args = {"--dict", "1", "str1", "--dict", "3", "--dict", "-1", "str4"};
    tapp.run();
    CHECK(3u == cv.size());
}

template <class T> class TApp_container_tuple : public TApp {
  public:
    using container_type = T;
    container_type cval{};
    TApp_container_tuple() : TApp() {}
};

using tup_obj = std::tuple<int, std::string, double>;

TEMPLATE_TEST_CASE("Container tuple",
                   "[optiontype]",
                   std::vector<tup_obj>,
                   std::deque<tup_obj>,
                   std::set<tup_obj>,
                   std::list<tup_obj>,
                   (std::map<int, std::pair<std::string, double>>),
                   (std::unordered_map<int, std::tuple<std::string, double>>)) {
    TApp tapp;
    TestType cv;

    (tapp.app).add_option("--dict", cv);

    tapp.args = {"--dict", "1", "str1", "4.3", "--dict", "3", "str3", "2.7"};

    tapp.run();
    CHECK(2u == cv.size());

    tapp.args = {"--dict", "1", "str1", "4.3", "--dict", "3", "str3", "2.7", "--dict", "-1", "str4", "-1.87"};
    tapp.run();
    CHECK(3u == cv.size());
}

using icontainer1 = std::vector<int>;
using icontainer2 = std::list<int>;
using icontainer3 = std::set<int>;
using icontainer4 = std::pair<int, std::vector<int>>;

TEMPLATE_TEST_CASE("Container container",
                   "[optiontype]",
                   std::vector<icontainer1>,
                   std::list<icontainer1>,
                   std::set<icontainer1>,
                   std::deque<icontainer1>,
                   std::vector<icontainer2>,
                   std::list<icontainer2>,
                   std::set<icontainer2>,
                   std::deque<icontainer2>,
                   std::vector<icontainer3>,
                   std::list<icontainer3>,
                   std::set<icontainer3>,
                   std::deque<icontainer3>) {
    TApp tapp;
    TestType cv;

    (tapp.app).add_option("--dict", cv);

    tapp.args = {"--dict", "1", "2", "4", "--dict", "3", "1"};

    tapp.run();
    CHECK(2u == cv.size());

    tapp.args = {"--dict", "1", "2", "4", "--dict", "3", "1", "--dict", "3", "--dict",
                 "3",      "3", "3", "3", "3",      "3", "3", "3",      "3", "-3"};
    tapp.run();
    CHECK(4u == cv.size());
}

TEST_CASE_METHOD(TApp, "containerContainer", "[optiontype]") {

    std::vector<icontainer4> cv;
    app.add_option("--dict", cv);

    args = {"--dict", "1", "2", "4", "--dict", "3", "1"};

    run();
    CHECK(2u == cv.size());

    args = {"--dict", "1", "2", "4", "--dict", "3", "1", "--dict", "3", "",  "--dict",
            "3",      "3", "3", "3", "3",      "3", "3", "3",      "3", "-3"};
    run();
    CHECK(4u == cv.size());
}

TEST_CASE_METHOD(TApp, "unknownContainerWrapper", "[optiontype]") {

    class vopt {
      public:
        vopt() = default;
        explicit vopt(std::vector<double> vdub) : val_{std::move(vdub)} {};
        std::vector<double> val_{};
    };

    vopt cv;
    app.add_option<vopt, std::vector<double>>("--vv", cv);

    args = {"--vv", "1", "2", "4"};

    run();
    CHECK(3u == cv.val_.size());
    args = {"--vv", ""};

    run();
    CHECK(cv.val_.empty());
}

TEST_CASE_METHOD(TApp, "tupleTwoVectors", "[optiontype]") {

    std::tuple<std::vector<int>, std::vector<int>> cv;
    app.add_option("--vv", cv);

    args = {"--vv", "1", "2", "4"};

    run();
    CHECK(3U == std::get<0>(cv).size());
    CHECK(std::get<1>(cv).empty());

    args = {"--vv", "1", "2", "%%", "4", "4", "5"};

    run();
    CHECK(2U == std::get<0>(cv).size());
    CHECK(3U == std::get<1>(cv).size());
}

TEST_CASE_METHOD(TApp, "vectorSingleArg", "[optiontype]") {

    std::vector<int> cv;
    app.add_option("-c", cv)->allow_extra_args(false);
    std::string extra;
    app.add_option("args", extra);
    args = {"-c", "1", "-c", "2", "4"};

    run();
    CHECK(2U == cv.size());
    CHECK("4" == extra);
}

TEST_CASE_METHOD(TApp, "vectorEmptyArg", "[optiontype]") {

    std::vector<std::string> cv{"test"};
    app.add_option("-c", cv);
    args = {"-c", "test1", "[]"};

    run();
    CHECK(cv.size() == 1);
    args = {"-c", "test1", "[[]]"};

    run();
    CHECK(cv.size() == 2);
    CHECK(cv[1] == "[]");
}

TEST_CASE_METHOD(TApp, "vectorDoubleArg", "[optiontype]") {

    std::vector<std::pair<int, std::string>> cv;
    app.add_option("-c", cv)->allow_extra_args(false);
    std::vector<std::string> extras;
    app.add_option("args", extras);
    args = {"-c", "1", "bob", "-c", "2", "apple", "4", "key"};

    run();
    CHECK(2U == cv.size());
    CHECK(2U == extras.size());
}

TEST_CASE_METHOD(TApp, "vectorEmpty", "[optiontype]") {

    std::vector<std::string> cv{};
    app.add_option("-c", cv)->expected(0, 2);

    args = {"-c", "{}"};

    run();
    CHECK(cv.empty());
}

TEST_CASE_METHOD(TApp, "vectorVectorArg", "[optiontype]") {

    std::vector<std::vector<std::string>> cv{};
    app.add_option("-c", cv);
    args = {"-c", "[[a,b]]"};

    run();
    CHECK(cv.size() == 1);
    CHECK(cv[0].size() == 2);
    CHECK(cv[0][0] == "a");
}

TEST_CASE_METHOD(TApp, "OnParseCall", "[optiontype]") {

    int cnt{0};

    auto *opt = app.add_option("-c",
                               [&cnt](const CLI::results_t &) {
                                   ++cnt;
                                   return true;
                               })
                    ->expected(1, 20)
                    ->trigger_on_parse();
    std::vector<std::string> extras;
    app.add_option("args", extras);
    args = {"-c", "1", "-c", "2", "-c", "3"};
    CHECK(opt->get_trigger_on_parse());
    run();
    CHECK(3 == cnt);
}

TEST_CASE_METHOD(TApp, "OnParseCallPositional", "[optiontype]") {

    int cnt{0};

    auto *opt = app.add_option("pos",
                               [&cnt](const CLI::results_t &) {
                                   ++cnt;
                                   return true;
                               })
                    ->trigger_on_parse()
                    ->allow_extra_args();
    args = {"1", "2", "3"};
    CHECK(opt->get_trigger_on_parse());
    run();
    CHECK(3 == cnt);
}

TEST_CASE_METHOD(TApp, "OnParseCallVector", "[optiontype]") {

    std::vector<std::string> vec;

    app.add_option("-c", vec)->trigger_on_parse();
    args = {"-c", "1", "2", "3", "-c", "2", "-c", "3", "4", "5"};
    run();
    CHECK(vec.size() == 3U);
}

TEST_CASE_METHOD(TApp, "force_callback", "[optiontype]") {

    int cnt{0};

    auto *opt = app.add_option("-c",
                               [&cnt](const CLI::results_t &) {
                                   ++cnt;
                                   return true;
                               })
                    ->expected(1, 20)
                    ->force_callback()
                    ->default_str("5");
    std::vector<std::string> extras;
    app.add_option("args", extras);
    args = {};
    CHECK(opt->get_force_callback());
    run();
    CHECK(1 == cnt);
    cnt = 0;
    args = {"-c", "10"};
    run();
    CHECK(1 == cnt);
}

TEST_CASE_METHOD(TApp, "force_callback2", "[optiontype]") {

    int cnt{0};

    app.add_option("-c", cnt)->force_callback()->default_val(5);
    args = {};
    run();
    CHECK(5 == cnt);
}

TEST_CASE_METHOD(TApp, "force_callback3", "[optiontype]") {

    int cnt{10};

    app.add_option("-c", cnt)->force_callback();
    args = {};
    run();
    CHECK(0 == cnt);
}
