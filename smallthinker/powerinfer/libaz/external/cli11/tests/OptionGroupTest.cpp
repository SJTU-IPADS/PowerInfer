// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <memory>
#include <string>
#include <vector>

using vs_t = std::vector<std::string>;

TEST_CASE_METHOD(TApp, "BasicOptionGroup", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res = 0;
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);

    args = {"--test1", "5"};
    run();
    CHECK(5 == res);
    CHECK(1u == app.count_all());
}

TEST_CASE_METHOD(TApp, "OptionGroupInvalidNames", "[optiongroup]") {
    CHECK_THROWS_AS(app.add_option_group("clusters\ncluster2", "description"), CLI::IncorrectConstruction);

    std::string groupName("group1");
    groupName += '\0';
    groupName.append("group2");

    CHECK_THROWS_AS(app.add_option_group(groupName), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupExact", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(1);
    args = {"--test1", "5"};
    run();
    CHECK(5 == res);

    args = {"--test1", "5", "--test2", "4"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    std::string help = ogroup->help();
    auto exactloc = help.find("[Exactly 1");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupExactTooMany", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(10);
    args = {"--test1", "5"};
    CHECK_THROWS_AS(run(), CLI::InvalidError);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMinMax", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(1, 1);
    args = {"--test1", "5"};
    run();
    CHECK(5 == res);

    args = {"--test1", "5", "--test2", "4"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    std::string help = ogroup->help();
    auto exactloc = help.find("[Exactly 1");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMinMaxDifferent", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(1, 2);
    args = {"--test1", "5"};
    run();
    CHECK(5 == res);

    args = {"--test1", "5", "--test2", "4"};
    CHECK_NOTHROW(run());
    CHECK(2u == app.count_all());

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    std::string help = ogroup->help();
    auto exactloc = help.find("[Between 1 and 2");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMinMaxDifferentReversed", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(2, 1);
    CHECK(2u == ogroup->get_require_option_min());
    CHECK(1u == ogroup->get_require_option_max());
    args = {"--test1", "5"};
    CHECK_THROWS_AS(run(), CLI::InvalidError);
    ogroup->require_option(1, 2);
    CHECK_NOTHROW(run());
    CHECK(5 == res);
    CHECK(1u == ogroup->get_require_option_min());
    CHECK(2u == ogroup->get_require_option_max());
    args = {"--test1", "5", "--test2", "4"};
    CHECK_NOTHROW(run());

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    std::string help = ogroup->help();
    auto exactloc = help.find("[Between 1 and 2");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMax", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2 = 0;
    app.add_option("--option", val2);
    ogroup->require_option(-2);
    args = {"--test1", "5"};
    run();
    CHECK(5 == res);

    args = {"--option", "9"};
    CHECK_NOTHROW(run());

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    std::string help = ogroup->help();
    auto exactloc = help.find("[At most 2");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMax1", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(-1);
    args = {"--test1", "5"};
    run();
    CHECK(5 == res);

    args = {"--option", "9"};
    CHECK_NOTHROW(run());

    args = {"--test1", "5", "--test2", "4"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    std::string help = ogroup->help();
    auto exactloc = help.find("[At most 1");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMin", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option();

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_NOTHROW(run());

    std::string help = ogroup->help();
    auto exactloc = help.find("[At least 1");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "integratedOptionGroup", "[optiongroup]") {
    auto *ogroup = app.add_option_group("+clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option();

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_NOTHROW(run());

    auto options = app.get_options();
    CHECK(options.size() == 5);
    const CLI::App *capp = &app;
    auto coptions = capp->get_options();
    CHECK(coptions.size() == 5);
    std::string help = app.help();
    auto exactloc = help.find("clusters");
    CHECK(std::string::npos == exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupExact2", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(2);

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test3=5"};
    CHECK_NOTHROW(run());

    std::string help = ogroup->help();
    auto exactloc = help.find("[Exactly 2");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMin2", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);
    ogroup->add_option("--test2", res);
    ogroup->add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);
    ogroup->require_option(2, 0);

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_NOTHROW(run());

    std::string help = ogroup->help();
    auto exactloc = help.find("[At least 2");
    CHECK(std::string::npos != exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMinMoved", "[optiongroup]") {

    int res{0};
    auto *opt1 = app.add_option("--test1", res);
    auto *opt2 = app.add_option("--test2", res);
    auto *opt3 = app.add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);

    auto *ogroup = app.add_option_group("clusters");
    ogroup->require_option();
    ogroup->add_option(opt1);
    ogroup->add_option(opt2);
    ogroup->add_option(opt3);

    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_NOTHROW(run());

    std::string help = app.help();
    auto exactloc = help.find("[At least 1");
    auto oloc = help.find("--test1");
    CHECK(std::string::npos != exactloc);
    CHECK(std::string::npos != oloc);
    CHECK(oloc > exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupMinMovedAsGroup", "[optiongroup]") {

    int res{0};
    auto *opt1 = app.add_option("--test1", res);
    auto *opt2 = app.add_option("--test2", res);
    auto *opt3 = app.add_option("--test3", res);
    int val2{0};
    app.add_option("--option", val2);

    auto *ogroup = app.add_option_group("clusters");
    ogroup->require_option();
    ogroup->add_options(opt1, opt2, opt3);

    CHECK_THROWS_AS(ogroup->add_options(opt1), CLI::OptionNotFound);
    args = {"--option", "9"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--test1", "5", "--test2", "4", "--test3=5"};
    CHECK_NOTHROW(run());

    std::string help = app.help();
    auto exactloc = help.find("[At least 1");
    auto oloc = help.find("--test1");
    CHECK(std::string::npos != exactloc);
    CHECK(std::string::npos != oloc);
    CHECK(oloc > exactloc);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupAddFailures", "[optiongroup]") {

    int res{0};
    auto *opt1 = app.add_option("--test1", res);
    app.set_config("--config");
    int val2{0};
    app.add_option("--option", val2);

    auto *ogroup = app.add_option_group("clusters");
    CHECK_THROWS_AS(ogroup->add_options(app.get_config_ptr()), CLI::OptionAlreadyAdded);
    CHECK_THROWS_AS(ogroup->add_options(app.get_help_ptr()), CLI::OptionAlreadyAdded);

    auto *sub = app.add_subcommand("sub", "subcommand");
    auto *opt2 = sub->add_option("--option2", val2);

    CHECK_THROWS_AS(ogroup->add_option(opt2), CLI::OptionNotFound);

    CHECK_THROWS_AS(ogroup->add_options(nullptr), CLI::OptionNotFound);

    ogroup->add_option(opt1);

    auto *opt3 = app.add_option("--test1", res);

    CHECK_THROWS_AS(ogroup->add_option(opt3), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "BasicOptionGroupScrewedUpMove", "[optiongroup]") {

    int res{0};
    auto *opt1 = app.add_option("--test1", res);
    auto *opt2 = app.add_option("--test2", res);
    int val2{0};
    app.add_option("--option", val2);

    auto *ogroup = app.add_option_group("clusters");
    ogroup->require_option();
    auto *ogroup2 = ogroup->add_option_group("clusters2");
    CHECK_THROWS_AS(ogroup2->add_options(opt1, opt2), CLI::OptionNotFound);

    CLI::Option_group EmptyGroup("description", "new group", nullptr);

    CHECK_THROWS_AS(EmptyGroup.add_option(opt2), CLI::OptionNotFound);
    CHECK_THROWS_AS(app._move_option(opt2, ogroup2), CLI::OptionNotFound);
}

TEST_CASE_METHOD(TApp, "InvalidOptions", "[optiongroup]") {
    auto *ogroup = app.add_option_group("clusters");
    CLI::Option *opt = nullptr;
    CHECK_THROWS_AS(ogroup->excludes(opt), CLI::OptionNotFound);
    CLI::App *app_p = nullptr;
    CHECK_THROWS_AS(ogroup->excludes(app_p), CLI::OptionNotFound);
    CHECK_THROWS_AS(ogroup->excludes(ogroup), CLI::OptionNotFound);
    CHECK_THROWS_AS(ogroup->add_option(opt), CLI::OptionNotFound);
}

TEST_CASE_METHOD(TApp, "OptionGroupInheritedOptionDefaults", "[optiongroup]") {
    app.option_defaults()->ignore_case();
    auto *ogroup = app.add_option_group("clusters");
    int res{0};
    ogroup->add_option("--test1", res);

    args = {"--Test1", "5"};
    run();
    CHECK(5 == res);
    CHECK(1u == app.count_all());
}

struct ManyGroups : public TApp {

    CLI::Option_group *main{nullptr};
    CLI::Option_group *g1{nullptr};
    CLI::Option_group *g2{nullptr};
    CLI::Option_group *g3{nullptr};
    std::string name1{};
    std::string name2{};
    std::string name3{};
    std::string val1{};
    std::string val2{};
    std::string val3{};

    ManyGroups(const ManyGroups &) = delete;
    ManyGroups &operator=(const ManyGroups &) = delete;

    ManyGroups() {
        main = app.add_option_group("main", "the main outer group");
        g1 = main->add_option_group("g1", "group1 description");
        g2 = main->add_option_group("g2", "group2 description");
        g3 = main->add_option_group("g3", "group3 description");
        g1->add_option("--name1", name1)->required();
        g1->add_option("--val1", val1);
        g2->add_option("--name2", name2)->required();
        g2->add_option("--val2", val2);
        g3->add_option("--name3", name3)->required();
        g3->add_option("--val3", val3);
    }

    void remove_required() {  // NOLINT(readability-make-member-function-const)
        g1->get_option("--name1")->required(false);
        g2->get_option("--name2")->required(false);
        g3->get_option("--name3")->required(false);
        g1->required(false);
        g2->required(false);
        g3->required(false);
    }
};

TEST_CASE_METHOD(ManyGroups, "SingleGroup", "[optiongroup]") {
    // only 1 group can be used
    main->require_option(1);
    args = {"--name1", "test"};
    run();
    CHECK("test" == name1);

    args = {"--name2", "test", "--val2", "tval"};

    run();
    CHECK("tval" == val2);

    args = {"--name1", "test", "--val2", "tval"};

    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(ManyGroups, "getGroup", "[optiongroup]") {
    auto *mn = app.get_option_group("main");
    CHECK(mn == main);
    CHECK_THROWS_AS(app.get_option_group("notfound"), CLI::OptionNotFound);
}

TEST_CASE_METHOD(ManyGroups, "ExcludesGroup", "[optiongroup]") {
    // only 1 group can be used
    g1->excludes(g2);
    g1->excludes(g3);
    args = {"--name1", "test"};
    run();
    CHECK("test" == name1);

    args = {"--name1", "test", "--name2", "test2"};

    CHECK_THROWS_AS(run(), CLI::ExcludesError);

    CHECK(g1->remove_excludes(g2));
    CHECK_NOTHROW(run());
    CHECK(!g1->remove_excludes(g1));
    CHECK(!g1->remove_excludes(g2));
}

TEST_CASE_METHOD(ManyGroups, "NeedsGroup", "[optiongroup]") {
    remove_required();
    // all groups needed if g1 is used
    g1->needs(g2);
    g1->needs(g3);
    args = {"--name1", "test"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);
    // other groups should run fine
    args = {"--name2", "test2"};

    run();
    // all three groups should be fine
    args = {"--name1", "test", "--name2", "test2", "--name3", "test3"};

    CHECK_NOTHROW(run());
}

// test adding an option group with existing subcommands to an app
TEST_CASE_METHOD(TApp, "ExistingSubcommandMatch", "[optiongroup]") {
    auto sshared = std::make_shared<CLI::Option_group>("documenting the subcommand", "sub1g", nullptr);
    auto *s1 = sshared->add_subcommand("sub1");
    auto *o1 = sshared->add_option_group("opt1");
    o1->add_subcommand("sub3")->alias("sub4");

    app.add_subcommand("sub1");

    try {
        app.add_subcommand(sshared);
        // this should throw the next line should never be reached
        CHECK(!true);
    } catch(const CLI::OptionAlreadyAdded &oaa) {
        CHECK_THAT(oaa.what(), Contains("sub1"));
    }
    sshared->remove_subcommand(s1);

    app.add_subcommand("sub3");
    // now check that the subsubcommand overlaps
    try {
        app.add_subcommand(sshared);
        // this should throw the next line should never be reached
        CHECK(!true);
    } catch(const CLI::OptionAlreadyAdded &oaa) {
        CHECK_THAT(oaa.what(), Contains("sub3"));
    }
}

TEST_CASE_METHOD(ManyGroups, "SingleGroupError", "[optiongroup]") {
    // only 1 group can be used
    main->require_option(1);
    args = {"--name1", "test", "--name2", "test3"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(ManyGroups, "AtMostOneGroup", "[optiongroup]") {
    // only 1 group can be used
    main->require_option(0, 1);
    args = {"--name1", "test", "--name2", "test3"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManyGroups, "AtLeastTwoGroups", "[optiongroup]") {
    // only 1 group can be used
    main->require_option(2, 0);
    args = {"--name1", "test", "--name2", "test3"};
    run();

    args = {"--name1", "test"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(ManyGroups, "BetweenOneAndTwoGroups", "[optiongroup]") {
    // only 1 group can be used
    main->require_option(1, 2);
    args = {"--name1", "test", "--name2", "test3"};
    run();

    args = {"--name1", "test"};
    run();

    args = {};
    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--name1", "test", "--name2", "test3", "--name3=test3"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(ManyGroups, "RequiredFirst", "[optiongroup]") {
    // only 1 group can be used
    remove_required();
    g1->required();

    CHECK(g1->get_required());
    CHECK(!g2->get_required());
    args = {"--name1", "test", "--name2", "test3"};
    run();

    args = {"--name2", "test"};
    try {
        run();
    } catch(const CLI::RequiredError &re) {
        CHECK_THAT(re.what(), Contains("g1"));
    }

    args = {"--name1", "test", "--name2", "test3", "--name3=test3"};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManyGroups, "DisableFirst", "[optiongroup]") {
    // only 1 group can be used if remove_required not used
    remove_required();
    g1->disabled();

    CHECK(g1->get_disabled());
    CHECK(!g2->get_disabled());
    args = {"--name2", "test"};

    run();

    args = {"--name1", "test", "--name2", "test3"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    g1->disabled(false);
    args = {"--name1", "test", "--name2", "test3", "--name3=test3"};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManyGroups, "SameSubcommand", "[optiongroup]") {
    // only 1 group can be used if remove_required not used
    remove_required();
    auto *sub1 = g1->add_subcommand("sub1")->disabled();
    auto *sub2 = g2->add_subcommand("sub1")->disabled();
    auto *sub3 = g3->add_subcommand("sub1");
    // so when the subcommands are disabled they can have the same name
    sub1->disabled(false);
    sub2->disabled(false);
    // if they are re-enabled they are not checked for overlap on enabling so they can have the same name
    args = {"sub1", "sub1", "sub1"};

    run();

    CHECK(*sub1);
    CHECK(*sub2);
    CHECK(*sub3);
    auto subs = app.get_subcommands();
    CHECK(3u == subs.size());
    CHECK(sub1 == subs[0]);
    CHECK(sub2 == subs[1]);
    CHECK(sub3 == subs[2]);

    args = {"sub1", "sub1", "sub1", "sub1"};
    // for the 4th and future ones they will route to the first one
    run();
    CHECK(2u == sub1->count());
    CHECK(1u == sub2->count());
    CHECK(1u == sub3->count());

    // subs should remain the same since the duplicate would not be registered there
    subs = app.get_subcommands();
    CHECK(3u == subs.size());
    CHECK(sub1 == subs[0]);
    CHECK(sub2 == subs[1]);
    CHECK(sub3 == subs[2]);
}
TEST_CASE_METHOD(ManyGroups, "CallbackOrder", "[optiongroup]") {
    // only 1 group can be used if remove_required not used
    remove_required();
    std::vector<int> callback_order;
    g1->callback([&callback_order]() { callback_order.push_back(1); });
    g2->callback([&callback_order]() { callback_order.push_back(2); });
    main->callback([&callback_order]() { callback_order.push_back(3); });

    args = {"--name2", "test"};
    run();
    CHECK(std::vector<int>({2, 3}) == callback_order);

    callback_order.clear();
    args = {"--name1", "t2", "--name2", "test"};
    g2->immediate_callback();
    run();
    CHECK(std::vector<int>({2, 1, 3}) == callback_order);
    callback_order.clear();

    args = {"--name2", "test", "--name1", "t2"};
    g2->immediate_callback(false);
    run();
    CHECK(std::vector<int>({1, 2, 3}) == callback_order);
}

// Test the fallthrough for extra arguments
TEST_CASE_METHOD(ManyGroups, "ExtrasFallDown", "[optiongroup]") {
    // only 1 group can be used if remove_required not used
    remove_required();

    args = {"--test1", "--flag", "extra"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    main->allow_extras();
    CHECK_NOTHROW(run());

    CHECK(3u == app.remaining_size(true));
    CHECK(3u == main->remaining_size());

    std::vector<std::string> extras{"--test1", "--flag", "extra"};
    CHECK(extras == app.remaining(true));
    CHECK(extras == main->remaining());
}

// Test the option Inheritance
TEST_CASE_METHOD(ManyGroups, "Inheritance", "[optiongroup]") {
    remove_required();
    g1->ignore_case();
    g1->ignore_underscore();
    auto *t2 = g1->add_subcommand("t2");
    args = {"T2", "t_2"};
    CHECK(t2->get_ignore_underscore());
    CHECK(t2->get_ignore_case());
    run();
    CHECK(2u == t2->count());
}

TEST_CASE_METHOD(ManyGroups, "Moving", "[optiongroup]") {
    remove_required();
    auto *mg = app.add_option_group("maing");
    mg->add_subcommand(g1);
    mg->add_subcommand(g2);

    CHECK(mg == g1->get_parent());
    CHECK(mg == g2->get_parent());
    CHECK(main == g3->get_parent());
}

struct ManyGroupsPreTrigger : public ManyGroups {
    std::size_t triggerMain{0u}, trigger1{87u}, trigger2{34u}, trigger3{27u};
    ManyGroupsPreTrigger() {
        remove_required();
        app.preparse_callback([this](std::size_t count) { triggerMain = count; });

        g1->preparse_callback([this](std::size_t count) { trigger1 = count; });
        g2->preparse_callback([this](std::size_t count) { trigger2 = count; });
        g3->preparse_callback([this](std::size_t count) { trigger3 = count; });
    }
};

TEST_CASE_METHOD(ManyGroupsPreTrigger, "PreTriggerTestsOptions", "[optiongroup]") {

    args = {"--name1", "test", "--name2", "test3"};
    run();
    CHECK(4u == triggerMain);
    CHECK(2u == trigger1);
    CHECK(0u == trigger2);
    CHECK(27u == trigger3);

    args = {"--name1", "test"};
    trigger2 = 34u;
    run();
    CHECK(2u == triggerMain);
    CHECK(0u == trigger1);
    CHECK(34u == trigger2);

    args = {};
    run();
    CHECK(0u == triggerMain);

    args = {"--name1", "test", "--val1", "45", "--name2", "test3", "--name3=test3", "--val2=37"};
    run();
    CHECK(8u == triggerMain);
    CHECK(6u == trigger1);
    CHECK(2u == trigger2);
    CHECK(1u == trigger3);
}

TEST_CASE_METHOD(ManyGroupsPreTrigger, "PreTriggerTestsPositionals", "[optiongroup]") {
    // only 1 group can be used
    g1->add_option("pos1");
    g2->add_option("pos2");
    g3->add_option("pos3");

    args = {"pos1"};
    run();
    CHECK(1u == triggerMain);
    CHECK(0u == trigger1);
    CHECK(34u == trigger2);
    CHECK(27u == trigger3);

    args = {"pos1", "pos2"};
    run();
    CHECK(2u == triggerMain);
    CHECK(1u == trigger1);
    CHECK(0u == trigger2);

    args = {"pos1", "pos2", "pos3"};
    run();
    CHECK(3u == triggerMain);
    CHECK(2u == trigger1);
    CHECK(1u == trigger2);
    CHECK(0u == trigger3);
}

TEST_CASE_METHOD(ManyGroupsPreTrigger, "PreTriggerTestsSubcommand", "[optiongroup]") {

    auto *sub1 = g1->add_subcommand("sub1")->fallthrough();
    g2->add_subcommand("sub2")->fallthrough();
    g3->add_subcommand("sub3")->fallthrough();

    std::size_t subtrigger = 0;
    sub1->preparse_callback([&subtrigger](std::size_t count) { subtrigger = count; });
    args = {"sub1"};
    run();
    CHECK(1u == triggerMain);
    CHECK(0u == trigger1);
    CHECK(34u == trigger2);
    CHECK(27u == trigger3);

    args = {"sub1", "sub2"};
    run();
    CHECK(2u == triggerMain);
    CHECK(1u == subtrigger);
    CHECK(1u == trigger1);
    CHECK(0u == trigger2);

    args = {"sub2", "sub3", "--name1=test", "sub1"};
    run();
    CHECK(4u == triggerMain);
    CHECK(1u == trigger1);
    CHECK(3u == trigger2);
    CHECK(1u == trigger3);
    // go until the sub1 command is given
}
