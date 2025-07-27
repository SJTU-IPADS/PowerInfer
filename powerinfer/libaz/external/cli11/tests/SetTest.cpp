// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

static_assert(CLI::is_shared_ptr<std::shared_ptr<int>>::value == true, "is_shared_ptr should work on shared pointers");
static_assert(CLI::is_shared_ptr<int *>::value == false, "is_shared_ptr should work on pointers");
static_assert(CLI::is_shared_ptr<int>::value == false, "is_shared_ptr should work on non-pointers");
static_assert(CLI::is_shared_ptr<const std::shared_ptr<int>>::value == true,
              "is_shared_ptr should work on const shared pointers");
static_assert(CLI::is_shared_ptr<const int *>::value == false, "is_shared_ptr should work on const pointers");
static_assert(CLI::is_shared_ptr<const int &>::value == false, "is_shared_ptr should work on const references");
static_assert(CLI::is_shared_ptr<int &>::value == false, "is_shared_ptr should work on non-const references");

static_assert(CLI::is_copyable_ptr<std::shared_ptr<int>>::value == true,
              "is_copyable_ptr should work on shared pointers");
static_assert(CLI::is_copyable_ptr<int *>::value == true, "is_copyable_ptr should work on pointers");
static_assert(CLI::is_copyable_ptr<int>::value == false, "is_copyable_ptr should work on non-pointers");
static_assert(CLI::is_copyable_ptr<const std::shared_ptr<int>>::value == true,
              "is_copyable_ptr should work on const shared pointers");
static_assert(CLI::is_copyable_ptr<const int *>::value == true, "is_copyable_ptr should work on const pointers");
static_assert(CLI::is_copyable_ptr<const int &>::value == false, "is_copyable_ptr should work on const references");
static_assert(CLI::is_copyable_ptr<int &>::value == false, "is_copyable_ptr should work on non-const references");

static_assert(CLI::detail::pair_adaptor<std::set<int>>::value == false, "Should not have pairs");
static_assert(CLI::detail::pair_adaptor<std::vector<std::string>>::value == false, "Should not have pairs");
static_assert(CLI::detail::pair_adaptor<std::map<int, int>>::value == true, "Should have pairs");
static_assert(CLI::detail::pair_adaptor<std::vector<std::pair<int, int>>>::value == true, "Should have pairs");

TEST_CASE_METHOD(TApp, "SimpleMaps", "[set]") {
    int value{0};
    std::map<std::string, int> map = {{"one", 1}, {"two", 2}};
    auto *opt = app.add_option("-s,--set", value)->transform(CLI::Transformer(map));
    args = {"-s", "one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK(1 == value);
}

TEST_CASE_METHOD(TApp, "StringStringMap", "[set]") {
    std::string value;
    std::map<std::string, std::string> map = {{"a", "b"}, {"b", "c"}};
    app.add_option("-s,--set", value)->transform(CLI::CheckedTransformer(map));
    args = {"-s", "a"};
    run();
    CHECK("b" == value);

    args = {"-s", "b"};
    run();
    CHECK("c" == value);

    args = {"-s", "c"};
    CHECK("c" == value);
}

TEST_CASE_METHOD(TApp, "StringStringMapNoModify", "[set]") {
    std::string value;
    std::map<std::string, std::string> map = {{"a", "b"}, {"b", "c"}};
    app.add_option("-s,--set", value)->check(CLI::IsMember(map));
    args = {"-s", "a"};
    run();
    CHECK("a" == value);

    args = {"-s", "b"};
    run();
    CHECK("b" == value);

    args = {"-s", "c"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

enum SimpleEnum { SE_one = 1, SE_two = 2 };

TEST_CASE_METHOD(TApp, "EnumMap", "[set]") {
    SimpleEnum value;  // NOLINT(cppcoreguidelines-init-variables)
    std::map<std::string, SimpleEnum> map = {{"one", SE_one}, {"two", SE_two}};
    auto *opt = app.add_option("-s,--set", value)->transform(CLI::Transformer(map));
    args = {"-s", "one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK(SE_one == value);
}

enum class SimpleEnumC { one = 1, two = 2 };

TEST_CASE_METHOD(TApp, "EnumCMap", "[set]") {
    SimpleEnumC value;  // NOLINT(cppcoreguidelines-init-variables)
    std::map<std::string, SimpleEnumC> map = {{"one", SimpleEnumC::one}, {"two", SimpleEnumC::two}};
    auto *opt = app.add_option("-s,--set", value)->transform(CLI::Transformer(map));
    args = {"-s", "one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK(SimpleEnumC::one == value);
}

TEST_CASE_METHOD(TApp, "structMap", "[set]") {
    struct tstruct {
        int val2;
        double val3;
        std::string v4;
    };
    std::string struct_name;
    std::map<std::string, struct tstruct> map = {{"sone", {4, 32.4, "foo"}}, {"stwo", {5, 99.7, "bar"}}};
    auto *opt = app.add_option("-s,--set", struct_name)->check(CLI::IsMember(map));
    args = {"-s", "sone"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("sone" == struct_name);

    args = {"-s", "sthree"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "structMapChange", "[set]") {
    struct tstruct {
        int val2;
        double val3;
        std::string v4;
    };
    std::string struct_name;
    std::map<std::string, struct tstruct> map = {{"sone", {4, 32.4, "foo"}}, {"stwo", {5, 99.7, "bar"}}};
    auto *opt = app.add_option("-s,--set", struct_name)
                    ->transform(CLI::IsMember(map, CLI::ignore_case, CLI::ignore_underscore, CLI::ignore_space));
    args = {"-s", "s one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("sone" == struct_name);

    args = {"-s", "sthree"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"-s", "S_t_w_o"};
    run();
    CHECK("stwo" == struct_name);
    args = {"-s", "S two"};
    run();
    CHECK("stwo" == struct_name);
}

TEST_CASE_METHOD(TApp, "structMapNoChange", "[set]") {
    struct tstruct {
        int val2;
        double val3;
        std::string v4;
    };
    std::string struct_name;
    std::map<std::string, struct tstruct> map = {{"sone", {4, 32.4, "foo"}}, {"stwo", {5, 99.7, "bar"}}};
    auto *opt = app.add_option("-s,--set", struct_name)
                    ->check(CLI::IsMember(map, CLI::ignore_case, CLI::ignore_underscore, CLI::ignore_space));
    args = {"-s", "SONE"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("SONE" == struct_name);

    args = {"-s", "sthree"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"-s", "S_t_w_o"};
    run();
    CHECK("S_t_w_o" == struct_name);

    args = {"-s", "S two"};
    run();
    CHECK("S two" == struct_name);
}

TEST_CASE_METHOD(TApp, "NonCopyableMap", "[set]") {

    std::string map_name;
    std::map<std::string, std::unique_ptr<double>> map;
    map["e1"].reset(new double(5.7));
    map["e3"].reset(new double(23.8));
    auto *opt = app.add_option("-s,--set", map_name)->check(CLI::IsMember(&map));
    args = {"-s", "e1"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("e1" == map_name);

    args = {"-s", "e45"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "NonCopyableMapWithFunction", "[set]") {

    std::string map_name;
    std::map<std::string, std::unique_ptr<double>> map;
    map["e1"].reset(new double(5.7));
    map["e3"].reset(new double(23.8));
    auto *opt = app.add_option("-s,--set", map_name)->transform(CLI::IsMember(&map, CLI::ignore_underscore));
    args = {"-s", "e_1"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("e1" == map_name);

    args = {"-s", "e45"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "NonCopyableMapNonStringMap", "[set]") {

    std::string map_name;
    std::map<int, std::unique_ptr<double>> map;
    map[4].reset(new double(5.7));
    map[17].reset(new double(23.8));
    auto *opt = app.add_option("-s,--set", map_name)->check(CLI::IsMember(&map));
    args = {"-s", "4"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("4" == map_name);

    args = {"-s", "e45"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "CopyableMapMove", "[set]") {

    std::string map_name;
    std::map<int, double> map;
    map[4] = 5.7;
    map[17] = 23.8;
    auto *opt = app.add_option("-s,--set", map_name)->check(CLI::IsMember(std::move(map)));
    args = {"-s", "4"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("4" == map_name);

    args = {"-s", "e45"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "SimpleSets", "[set]") {
    std::string value;
    auto *opt = app.add_option("-s,--set", value)->check(CLI::IsMember{std::set<std::string>({"one", "two", "three"})});
    args = {"-s", "one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("one" == value);
}

TEST_CASE_METHOD(TApp, "SimpleSetsPtrs", "[set]") {
    auto set = std::make_shared<std::set<std::string>>(std::set<std::string>{"one", "two", "three"});
    std::string value;
    auto *opt = app.add_option("-s,--set", value)->check(CLI::IsMember{set});
    args = {"-s", "one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("one" == value);

    set->insert("four");

    args = {"-s", "four"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("four" == value);
}

TEST_CASE_METHOD(TApp, "SimiShortcutSets", "[set]") {
    std::string value;
    auto *opt = app.add_option("--set", value)->check(CLI::IsMember({"one", "two", "three"}));
    args = {"--set", "one"};
    run();
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("one" == value);

    std::string value2;
    auto *opt2 = app.add_option("--set2", value2)->transform(CLI::IsMember({"One", "two", "three"}, CLI::ignore_case));
    args = {"--set2", "onE"};
    run();
    CHECK(app.count("--set2") == 1u);
    CHECK(opt2->count() == 1u);
    CHECK("One" == value2);

    std::string value3;
    auto *opt3 = app.add_option("--set3", value3)
                     ->transform(CLI::IsMember({"O_ne", "two", "three"}, CLI::ignore_case, CLI::ignore_underscore));
    args = {"--set3", "onE"};
    run();
    CHECK(app.count("--set3") == 1u);
    CHECK(opt3->count() == 1u);
    CHECK("O_ne" == value3);
}

TEST_CASE_METHOD(TApp, "SetFromCharStarArrayVector", "[set]") {
    constexpr const char *names[3]{"one", "two", "three"};  // NOLINT(modernize-avoid-c-arrays)
    std::string value;
    auto *opt = app.add_option("-s,--set", value)
                    ->check(CLI::IsMember{std::vector<std::string>(std::begin(names), std::end(names))});
    args = {"-s", "one"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK("one" == value);
}

TEST_CASE_METHOD(TApp, "OtherTypeSets", "[set]") {
    int value{0};
    std::vector<int> set = {2, 3, 4};
    auto *opt = app.add_option("--set", value)->check(CLI::IsMember(set));
    args = {"--set", "3"};
    run();
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK(3 == value);

    args = {"--set", "5"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    std::vector<int> set2 = {-2, 3, 4};
    auto *opt2 = app.add_option("--set2", value)->transform(CLI::IsMember(set2, [](int x) { return std::abs(x); }));
    args = {"--set2", "-3"};
    run();
    CHECK(app.count("--set2") == 1u);
    CHECK(opt2->count() == 1u);
    CHECK(3 == value);

    args = {"--set2", "-3"};
    run();
    CHECK(app.count("--set2") == 1u);
    CHECK(opt2->count() == 1u);
    CHECK(3 == value);

    args = {"--set2", "2"};
    run();
    CHECK(app.count("--set2") == 1u);
    CHECK(opt2->count() == 1u);
    CHECK(-2 == value);
}

TEST_CASE_METHOD(TApp, "NumericalSets", "[set]") {
    int value{0};
    auto *opt = app.add_option("-s,--set", value)->check(CLI::IsMember{std::set<int>({1, 2, 3})});
    args = {"-s", "1"};
    run();
    CHECK(app.count("-s") == 1u);
    CHECK(app.count("--set") == 1u);
    CHECK(opt->count() == 1u);
    CHECK(1 == value);
}

// Converted original set tests

TEST_CASE_METHOD(TApp, "SetWithDefaults", "[set]") {
    int someint{2};
    app.add_option("-a", someint)->capture_default_str()->check(CLI::IsMember({1, 2, 3, 4}));

    args = {"-a1", "-a2"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "SetWithDefaultsConversion", "[set]") {
    int someint{2};
    app.add_option("-a", someint)->capture_default_str()->check(CLI::IsMember({1, 2, 3, 4}));

    args = {"-a", "hi"};

    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "SetWithDefaultsIC", "[set]") {
    std::string someint = "ho";
    app.add_option("-a", someint)->capture_default_str()->check(CLI::IsMember({"Hi", "Ho"}));

    args = {"-aHi", "-aHo"};

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "InSet", "[set]") {

    std::string choice;
    app.add_option("-q,--quick", choice)->check(CLI::IsMember({"one", "two", "three"}));

    args = {"--quick", "two"};

    run();
    CHECK(choice == "two");

    args = {"--quick", "four"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "InSetWithDefault", "[set]") {

    std::string choice = "one";
    app.add_option("-q,--quick", choice)->capture_default_str()->check(CLI::IsMember({"one", "two", "three"}));

    run();
    CHECK(choice == "one");

    args = {"--quick", "two"};

    run();
    CHECK(choice == "two");

    args = {"--quick", "four"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "InCaselessSetWithDefault", "[set]") {

    std::string choice = "one";
    app.add_option("-q,--quick", choice)
        ->capture_default_str()
        ->transform(CLI::IsMember({"one", "two", "three"}, CLI::ignore_case));

    run();
    CHECK(choice == "one");

    args = {"--quick", "tWo"};

    run();
    CHECK(choice == "two");

    args = {"--quick", "four"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "InIntSet", "[set]") {

    int choice{0};
    app.add_option("-q,--quick", choice)->check(CLI::IsMember({1, 2, 3}));

    args = {"--quick", "2"};

    run();
    CHECK(choice == 2);

    args = {"--quick", "4"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "InIntSetWindows", "[set]") {

    int choice{0};
    app.add_option("-q,--quick", choice)->check(CLI::IsMember({1, 2, 3}));
    app.allow_windows_style_options();
    args = {"/q", "2"};

    run();
    CHECK(choice == 2);

    args = {"/q", "4"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"/q4"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "FailSet", "[set]") {

    int choice{0};
    app.add_option("-q,--quick", choice)->check(CLI::IsMember({1, 2, 3}));

    args = {"--quick", "3", "--quick=2"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--quick=hello"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "shortStringCheck", "[set]") {

    std::string choice;
    app.add_option("-q,--quick", choice)->check([](const std::string &v) {
        if(v.size() > 5) {
            return std::string{"string too long"};
        }
        return std::string{};
    });

    args = {"--quick", "3"};
    CHECK_NOTHROW(run());

    args = {"--quick=hello_goodbye"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "FailMutableSet", "[set]") {

    int choice{0};
    auto vals = std::make_shared<std::set<int>>(std::set<int>{1, 2, 3});
    app.add_option("-q,--quick", choice)->check(CLI::IsMember(vals));
    app.add_option("-s,--slow", choice)->capture_default_str()->check(CLI::IsMember(vals));

    args = {"--quick=hello"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--slow=hello"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "InSetIgnoreCase", "[set]") {

    std::string choice;
    app.add_option("-q,--quick", choice)->transform(CLI::IsMember({"one", "Two", "THREE"}, CLI::ignore_case));

    args = {"--quick", "One"};
    run();
    CHECK(choice == "one");

    args = {"--quick", "two"};
    run();
    CHECK(choice == "Two");

    args = {"--quick", "ThrEE"};
    run();
    CHECK(choice == "THREE");

    args = {"--quick", "four"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--quick=one", "--quick=two"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "InSetIgnoreCaseMutableValue", "[set]") {

    std::set<std::string> options{"one", "Two", "THREE"};
    std::string choice;
    app.add_option("-q,--quick", choice)->transform(CLI::IsMember(&options, CLI::ignore_case));

    args = {"--quick", "One"};
    run();
    CHECK(choice == "one");

    args = {"--quick", "two"};
    run();
    CHECK(choice == "Two");

    args = {"--quick", "ThrEE"};
    run();
    CHECK(choice == "THREE");

    options.clear();
    args = {"--quick", "ThrEE"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "InSetIgnoreCasePointer", "[set]") {

    auto *options = new std::set<std::string>{"one", "Two", "THREE"};
    std::string choice;
    app.add_option("-q,--quick", choice)->transform(CLI::IsMember(*options, CLI::ignore_case));

    args = {"--quick", "One"};
    run();
    CHECK(choice == "one");

    args = {"--quick", "two"};
    run();
    CHECK(choice == "Two");

    args = {"--quick", "ThrEE"};
    run();
    CHECK(choice == "THREE");

    delete options;
    args = {"--quick", "ThrEE"};
    run();
    CHECK(choice == "THREE");

    args = {"--quick", "four"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--quick=one", "--quick=two"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "NotInSetIgnoreCasePointer", "[set]") {

    auto *options = new std::set<std::string>{"one", "Two", "THREE"};
    std::string choice;
    app.add_option("-q,--quick", choice)->check(!CLI::IsMember(*options, CLI::ignore_case));

    args = {"--quick", "One"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--quick", "four"};
    run();
    CHECK("four" == choice);
}

TEST_CASE_METHOD(TApp, "InSetIgnoreUnderscore", "[set]") {

    std::string choice;
    app.add_option("-q,--quick", choice)
        ->transform(CLI::IsMember({"option_one", "option_two", "optionthree"}, CLI::ignore_underscore));

    args = {"--quick", "option_one"};
    run();
    CHECK(choice == "option_one");

    args = {"--quick", "optiontwo"};
    run();
    CHECK(choice == "option_two");

    args = {"--quick", "_option_thr_ee"};
    run();
    CHECK(choice == "optionthree");

    args = {"--quick", "Option4"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--quick=option_one", "--quick=option_two"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "InSetIgnoreCaseUnderscore", "[set]") {

    std::string choice;
    app.add_option("-q,--quick", choice)
        ->transform(
            CLI::IsMember({"Option_One", "option_two", "OptionThree"}, CLI::ignore_case, CLI::ignore_underscore));

    args = {"--quick", "option_one"};
    run();
    CHECK(choice == "Option_One");

    args = {"--quick", "OptionTwo"};
    run();
    CHECK(choice == "option_two");

    args = {"--quick", "_OPTION_thr_ee"};
    run();
    CHECK(choice == "OptionThree");

    args = {"--quick", "Option4"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--quick=option_one", "--quick=option_two"};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

// #113
TEST_CASE_METHOD(TApp, "AddRemoveSetItems", "[set]") {
    std::set<std::string> items{"TYPE1", "TYPE2", "TYPE3", "TYPE4", "TYPE5"};

    std::string type1, type2;
    app.add_option("--type1", type1)->check(CLI::IsMember(&items));
    app.add_option("--type2", type2)->capture_default_str()->check(CLI::IsMember(&items));

    args = {"--type1", "TYPE1", "--type2", "TYPE2"};

    run();
    CHECK("TYPE1" == type1);
    CHECK("TYPE2" == type2);

    items.insert("TYPE6");
    items.insert("TYPE7");

    items.erase("TYPE1");
    items.erase("TYPE2");

    args = {"--type1", "TYPE6", "--type2", "TYPE7"};
    run();
    CHECK("TYPE6" == type1);
    CHECK("TYPE7" == type2);

    args = {"--type1", "TYPE1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--type2", "TYPE2"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}

TEST_CASE_METHOD(TApp, "AddRemoveSetItemsNoCase", "[set]") {
    std::set<std::string> items{"TYPE1", "TYPE2", "TYPE3", "TYPE4", "TYPE5"};

    std::string type1, type2;
    app.add_option("--type1", type1)->transform(CLI::IsMember(&items, CLI::ignore_case));
    app.add_option("--type2", type2)->capture_default_str()->transform(CLI::IsMember(&items, CLI::ignore_case));

    args = {"--type1", "TYPe1", "--type2", "TyPE2"};

    run();
    CHECK("TYPE1" == type1);
    CHECK("TYPE2" == type2);

    items.insert("TYPE6");
    items.insert("TYPE7");

    items.erase("TYPE1");
    items.erase("TYPE2");

    args = {"--type1", "TyPE6", "--type2", "tYPE7"};
    run();
    CHECK("TYPE6" == type1);
    CHECK("TYPE7" == type2);

    args = {"--type1", "TYPe1"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);

    args = {"--type2", "TYpE2"};
    CHECK_THROWS_AS(run(), CLI::ValidationError);
}
