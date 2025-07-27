// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"
#include <cstdlib>
#include <string>
#include <vector>

TEST_CASE_METHOD(TApp, "AddingExistingShort", "[creation]") {
    CLI::Option *opt = app.add_flag("-c,--count");
    CHECK(std::vector<std::string>({"count"}) == opt->get_lnames());
    CHECK(std::vector<std::string>({"c"}) == opt->get_snames());

    CHECK_THROWS_AS(app.add_flag("--cat,-c"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingLong", "[creation]") {
    app.add_flag("-q,--count");
    CHECK_THROWS_AS(app.add_flag("--count,-c"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingShortNoCase", "[creation]") {
    app.add_flag("-C,--count")->ignore_case();
    CHECK_THROWS_AS(app.add_flag("--cat,-c"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingLongNoCase", "[creation]") {
    app.add_flag("-q,--count")->ignore_case();
    CHECK_THROWS_AS(app.add_flag("--Count,-c"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingNoCaseReversed", "[creation]") {
    app.add_flag("-c,--count")->ignore_case();
    CHECK_THROWS_AS(app.add_flag("--cat,-C"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingWithCase", "[creation]") {
    app.add_flag("-c,--count");
    CHECK_NOTHROW(app.add_flag("--Cat,-C"));
}

TEST_CASE_METHOD(TApp, "AddingExistingShortLong", "[creation]") {
    app.add_flag("-c");
    CHECK_THROWS_AS(app.add_flag("--c"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingLongShort", "[creation]") {
    app.add_flag("--c");
    CHECK_THROWS_AS(app.add_option("-c"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingWithCaseAfter", "[creation]") {
    auto *count = app.add_flag("-c,--count");
    app.add_flag("--Cat,-C");

    CHECK_THROWS_AS(count->ignore_case(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingWithCaseAfter2", "[creation]") {
    app.add_flag("-c,--count");
    auto *cat = app.add_flag("--Cat,-C");

    CHECK_THROWS_AS(cat->ignore_case(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingWithUnderscoreAfter", "[creation]") {
    auto *count = app.add_flag("--underscore");
    app.add_flag("--under_score");

    CHECK_THROWS_AS(count->ignore_underscore(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingExistingWithUnderscoreAfter2", "[creation]") {
    auto *count = app.add_flag("--under_score");
    app.add_flag("--underscore");

    CHECK_THROWS_AS(count->ignore_underscore(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "matchPositional", "[creation]") {
    app.add_option("firstoption");
    CHECK_THROWS_AS(app.add_option("--firstoption"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "matchPositional2", "[creation]") {
    app.add_option("--firstoption");
    CHECK_THROWS_AS(app.add_option("firstoption"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "matchPositionalInOptionGroup1", "[creation]") {

    auto *g1 = app.add_option_group("group_b");
    g1->add_option("--firstoption");
    CHECK_THROWS_AS(app.add_option("firstoption"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "matchPositionalInOptionGroup2", "[creation]") {

    app.add_option("firstoption");
    auto *g1 = app.add_option_group("group_b");
    CHECK_THROWS_AS(g1->add_option("--firstoption"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "matchPositionalInOptionGroup3", "[creation]") {

    app.add_option("f");
    auto *g1 = app.add_option_group("group_b");
    CHECK_THROWS_AS(g1->add_option("-f"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "AddingMultipleInfPositionals", "[creation]") {
    std::vector<std::string> one, two;
    app.add_option("one", one);
    app.add_option("two", two);

    CHECK_THROWS_AS(run(), CLI::InvalidError);
}

TEST_CASE_METHOD(TApp, "AddingMultipleInfPositionalsSubcom", "[creation]") {
    std::vector<std::string> one, two;
    CLI::App *below = app.add_subcommand("below");
    below->add_option("one", one);
    below->add_option("two", two);

    CHECK_THROWS_AS(run(), CLI::InvalidError);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatching", "[creation]") {
    app.add_subcommand("first");
    app.add_subcommand("second");
    app.add_subcommand("Second");
    CHECK_THROWS_AS(app.add_subcommand("first"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "RecoverSubcommands", "[creation]") {
    CLI::App *app1 = app.add_subcommand("app1");
    CLI::App *app2 = app.add_subcommand("app2");
    CLI::App *app3 = app.add_subcommand("app3");
    CLI::App *app4 = app.add_subcommand("app4");

    CHECK(std::vector<CLI::App *>({app1, app2, app3, app4}) == app.get_subcommands({}));
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithCase", "[creation]") {
    app.add_subcommand("first")->ignore_case();
    CHECK_THROWS_AS(app.add_subcommand("fIrst"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithCaseFirst", "[creation]") {
    app.ignore_case();
    app.add_subcommand("first");
    CHECK_THROWS_AS(app.add_subcommand("fIrst"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithUnderscore", "[creation]") {
    app.add_subcommand("first_option")->ignore_underscore();
    CHECK_THROWS_AS(app.add_subcommand("firstoption"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithUnderscoreFirst", "[creation]") {
    app.ignore_underscore();
    app.add_subcommand("first_option");
    CHECK_THROWS_AS(app.add_subcommand("firstoption"), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithCaseInplace", "[creation]") {
    app.add_subcommand("first");
    auto *first = app.add_subcommand("fIrst");

    CHECK_THROWS_AS(first->ignore_case(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithCaseInplace2", "[creation]") {
    auto *first = app.add_subcommand("first");
    app.add_subcommand("fIrst");

    CHECK_THROWS_AS(first->ignore_case(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithUnderscoreInplace", "[creation]") {
    app.add_subcommand("first_option");
    auto *first = app.add_subcommand("firstoption");

    CHECK_THROWS_AS(first->ignore_underscore(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomMatchingWithUnderscoreInplace2", "[creation]") {
    auto *first = app.add_subcommand("firstoption");
    app.add_subcommand("first_option");

    CHECK_THROWS_AS(first->ignore_underscore(), CLI::OptionAlreadyAdded);
}

TEST_CASE_METHOD(TApp, "MultipleSubcomNoMatchingInplace2", "[creation]") {
    auto *first = app.add_subcommand("first");
    auto *second = app.add_subcommand("second");

    CHECK_NOTHROW(first->ignore_case());
    CHECK_NOTHROW(second->ignore_case());
}

TEST_CASE_METHOD(TApp, "MultipleSubcomNoMatchingInplaceUnderscore2", "[creation]") {
    auto *first = app.add_subcommand("first_option");
    auto *second = app.add_subcommand("second_option");

    CHECK_NOTHROW(first->ignore_underscore());
    CHECK_NOTHROW(second->ignore_underscore());
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionFlagPositional1", "[creation]") {
    // This wants to be one line with clang-format
    CHECK_THROWS_AS(app.add_flag("cat"), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionFlagPositional2", "[creation]") {
    int x{0};
    CHECK_THROWS_AS(app.add_flag("cat", x), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionFlagPositional3", "[creation]") {
    bool x{false};
    CHECK_THROWS_AS(app.add_flag("cat", x), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionNeedsCannotFind", "[creation]") {
    auto *cat = app.add_flag("--cat");
    CHECK_THROWS_AS(cat->needs("--nothing"), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionExcludesCannotFind", "[creation]") {
    auto *cat = app.add_flag("--cat");
    CHECK_THROWS_AS(cat->excludes("--nothing"), CLI::IncorrectConstruction);
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionDuplicateNeeds", "[creation]") {
    auto *cat = app.add_flag("--cat");
    auto *other = app.add_flag("--other");
    REQUIRE_NOTHROW(cat->needs(other));
    // duplicated needs is redundant but not an error
    CHECK_NOTHROW(cat->needs(other));
}

TEST_CASE_METHOD(TApp, "IncorrectConstructionDuplicateNeedsTxt", "[creation]") {
    auto *cat = app.add_flag("--cat");
    app.add_flag("--other");
    REQUIRE_NOTHROW(cat->needs("--other"));
    // duplicate needs is redundant but not an error
    CHECK_NOTHROW(cat->needs("--other"));
}

// Now allowed
TEST_CASE_METHOD(TApp, "CorrectConstructionDuplicateExcludes", "[creation]") {
    auto *cat = app.add_flag("--cat");
    auto *other = app.add_flag("--other");
    REQUIRE_NOTHROW(cat->excludes(other));
    REQUIRE_NOTHROW(other->excludes(cat));
}

// Now allowed
TEST_CASE_METHOD(TApp, "CorrectConstructionDuplicateExcludesTxt", "[creation]") {
    auto *cat = app.add_flag("--cat");
    auto *other = app.add_flag("--other");
    REQUIRE_NOTHROW(cat->excludes("--other"));
    REQUIRE_NOTHROW(other->excludes("--cat"));
}

TEST_CASE_METHOD(TApp, "CheckName", "[creation]") {
    auto *long1 = app.add_flag("--long1");
    auto *long2 = app.add_flag("--Long2");
    auto *short1 = app.add_flag("-a");
    auto *short2 = app.add_flag("-B");
    int x{0}, y{0};
    auto *pos1 = app.add_option("pos1", x);
    auto *pos2 = app.add_option("pOs2", y);

    CHECK(long1->check_name("--long1"));
    CHECK(!long1->check_name("--lonG1"));

    CHECK(long2->check_name("--Long2"));
    CHECK(!long2->check_name("--long2"));

    CHECK(short1->check_name("-a"));
    CHECK(!short1->check_name("-A"));

    CHECK(short2->check_name("-B"));
    CHECK(!short2->check_name("-b"));

    CHECK(pos1->check_name("pos1"));
    CHECK(!pos1->check_name("poS1"));

    CHECK(pos2->check_name("pOs2"));
    CHECK(!pos2->check_name("pos2"));
}

TEST_CASE_METHOD(TApp, "CheckNameNoCase", "[creation]") {
    auto *long1 = app.add_flag("--long1")->ignore_case();
    auto *long2 = app.add_flag("--Long2")->ignore_case();
    auto *short1 = app.add_flag("-a")->ignore_case();
    auto *short2 = app.add_flag("-B")->ignore_case();
    int x{0}, y{0};
    auto *pos1 = app.add_option("pos1", x)->ignore_case();
    auto *pos2 = app.add_option("pOs2", y)->ignore_case();

    CHECK(long1->check_name("--long1"));
    CHECK(long1->check_name("--lonG1"));

    CHECK(long2->check_name("--Long2"));
    CHECK(long2->check_name("--long2"));

    CHECK(short1->check_name("-a"));
    CHECK(short1->check_name("-A"));

    CHECK(short2->check_name("-B"));
    CHECK(short2->check_name("-b"));

    CHECK(pos1->check_name("pos1"));
    CHECK(pos1->check_name("poS1"));

    CHECK(pos2->check_name("pOs2"));
    CHECK(pos2->check_name("pos2"));
}

TEST_CASE_METHOD(TApp, "CheckNameNoUnderscore", "[creation]") {
    auto *long1 = app.add_flag("--longoption1")->ignore_underscore();
    auto *long2 = app.add_flag("--long_option2")->ignore_underscore();

    int x{0}, y{0};
    auto *pos1 = app.add_option("pos_option_1", x)->ignore_underscore();
    auto *pos2 = app.add_option("posoption2", y)->ignore_underscore();

    CHECK(long1->check_name("--long_option1"));
    CHECK(long1->check_name("--longoption_1"));
    CHECK(long1->check_name("--longoption1"));
    CHECK(long1->check_name("--long__opt_ion__1"));
    CHECK(long1->check_name("--__l_o_n_g_o_p_t_i_o_n_1"));

    CHECK(long2->check_name("--long_option2"));
    CHECK(long2->check_name("--longoption2"));
    CHECK(long2->check_name("--longoption_2"));
    CHECK(long2->check_name("--long__opt_ion__2"));
    CHECK(long2->check_name("--__l_o_n_go_p_t_i_o_n_2__"));

    CHECK(pos1->check_name("pos_option1"));
    CHECK(pos1->check_name("pos_option_1"));
    CHECK(pos1->check_name("pos_o_p_t_i_on_1"));
    CHECK(pos1->check_name("posoption1"));

    CHECK(pos2->check_name("pos_option2"));
    CHECK(pos2->check_name("pos_option_2"));
    CHECK(pos2->check_name("pos_o_p_t_i_on_2"));
    CHECK(pos2->check_name("posoption2"));
}

TEST_CASE_METHOD(TApp, "CheckNameNoCaseNoUnderscore", "[creation]") {
    auto *long1 = app.add_flag("--LongoptioN1")->ignore_underscore()->ignore_case();
    auto *long2 = app.add_flag("--long_Option2")->ignore_case()->ignore_underscore();

    int x{0}, y{0};
    auto *pos1 = app.add_option("pos_Option_1", x)->ignore_underscore()->ignore_case();
    auto *pos2 = app.add_option("posOption2", y)->ignore_case()->ignore_underscore();

    CHECK(long1->check_name("--Long_Option1"));
    CHECK(long1->check_name("--lONgoption_1"));
    CHECK(long1->check_name("--LongOption1"));
    CHECK(long1->check_name("--long__Opt_ion__1"));
    CHECK(long1->check_name("--__l_o_N_g_o_P_t_i_O_n_1"));

    CHECK(long2->check_name("--long_Option2"));
    CHECK(long2->check_name("--LongOption2"));
    CHECK(long2->check_name("--longOPTION_2"));
    CHECK(long2->check_name("--long__OPT_ion__2"));
    CHECK(long2->check_name("--__l_o_n_GO_p_t_i_o_n_2__"));

    CHECK(pos1->check_name("POS_Option1"));
    CHECK(pos1->check_name("pos_option_1"));
    CHECK(pos1->check_name("pos_o_p_t_i_on_1"));
    CHECK(pos1->check_name("posoption1"));

    CHECK(pos2->check_name("pos_option2"));
    CHECK(pos2->check_name("pos_OPTION_2"));
    CHECK(pos2->check_name("poS_o_p_T_I_on_2"));
    CHECK(pos2->check_name("PosOption2"));
}

TEST_CASE_METHOD(TApp, "PreSpaces", "[creation]") {
    int x{0};
    auto *myapp = app.add_option(" -a, --long, other", x);

    CHECK(myapp->check_lname("long"));
    CHECK(myapp->check_sname("a"));
    CHECK(myapp->check_name("other"));
}

TEST_CASE_METHOD(TApp, "AllSpaces", "[creation]") {
    int x{0};
    auto *myapp = app.add_option(" -a , --long , other ", x);

    CHECK(myapp->check_lname("long"));
    CHECK(myapp->check_sname("a"));
    CHECK(myapp->check_name("other"));
}

TEST_CASE_METHOD(TApp, "OptionFromDefaults", "[creation]") {
    app.option_defaults()->required();

    // Options should remember defaults
    int x{0};
    auto *opt = app.add_option("--simple", x);
    CHECK(opt->get_required());

    // Flags cannot be required
    auto *flag = app.add_flag("--other");
    CHECK(!flag->get_required());

    app.option_defaults()->required(false);
    auto *opt2 = app.add_option("--simple2", x);
    CHECK(!opt2->get_required());

    app.option_defaults()->required()->ignore_case();

    auto *opt3 = app.add_option("--simple3", x);
    CHECK(opt3->get_required());
    CHECK(opt3->get_ignore_case());

    app.option_defaults()->required()->ignore_underscore();

    auto *opt4 = app.add_option("--simple4", x);
    CHECK(opt4->get_required());
    CHECK(opt4->get_ignore_underscore());
}

TEST_CASE_METHOD(TApp, "OptionFromDefaultsSubcommands", "[creation]") {
    // Initial defaults
    CHECK(!app.option_defaults()->get_required());
    CHECK(CLI::MultiOptionPolicy::Throw == app.option_defaults()->get_multi_option_policy());
    CHECK(!app.option_defaults()->get_ignore_case());
    CHECK(!app.option_defaults()->get_ignore_underscore());
    CHECK(!app.option_defaults()->get_disable_flag_override());
    CHECK(app.option_defaults()->get_configurable());
    CHECK("OPTIONS" == app.option_defaults()->get_group());

    app.option_defaults()
        ->required()
        ->multi_option_policy(CLI::MultiOptionPolicy::TakeLast)
        ->ignore_case()
        ->ignore_underscore()
        ->configurable(false)
        ->disable_flag_override()
        ->group("Something");

    auto *app2 = app.add_subcommand("app2");

    CHECK(app2->option_defaults()->get_required());
    CHECK(CLI::MultiOptionPolicy::TakeLast == app2->option_defaults()->get_multi_option_policy());
    CHECK(app2->option_defaults()->get_ignore_case());
    CHECK(app2->option_defaults()->get_ignore_underscore());
    CHECK(!app2->option_defaults()->get_configurable());
    CHECK(app.option_defaults()->get_disable_flag_override());
    CHECK("Something" == app2->option_defaults()->get_group());
}

TEST_CASE_METHOD(TApp, "GetNameCheck", "[creation]") {
    int x{0};
    auto *a = app.add_flag("--that");
    auto *b = app.add_flag("-x");
    auto *c = app.add_option("pos", x);
    auto *d = app.add_option("one,-o,--other", x);

    CHECK("--that" == a->get_name(false, true));
    CHECK("-x" == b->get_name(false, true));
    CHECK("pos" == c->get_name(false, true));

    CHECK("--other" == d->get_name());
    CHECK("--other" == d->get_name(false, false));
    CHECK("-o,--other" == d->get_name(false, true));
    CHECK("one,-o,--other" == d->get_name(true, true));
    CHECK("one" == d->get_name(true, false));
}

TEST_CASE_METHOD(TApp, "SubcommandDefaults", "[creation]") {
    // allow_extras, prefix_command, ignore_case, fallthrough, group, min/max subcommand, validate_positionals

    // Initial defaults
    CHECK(!app.get_allow_extras());
    CHECK(!app.get_prefix_command());
    CHECK(!app.get_immediate_callback());
    CHECK(!app.get_ignore_case());
    CHECK(!app.get_ignore_underscore());
#ifdef _WIN32
    CHECK(app.get_allow_windows_style_options());
#else
    CHECK(!app.get_allow_windows_style_options());
#endif
    CHECK(!app.get_fallthrough());
    CHECK(!app.get_configurable());
    CHECK(!app.get_validate_positionals());

    CHECK(app.get_usage().empty());
    CHECK(app.get_footer().empty());
    CHECK("SUBCOMMANDS" == app.get_group());
    CHECK(0u == app.get_require_subcommand_min());
    CHECK(0u == app.get_require_subcommand_max());

    app.allow_extras();
    app.prefix_command();
    app.immediate_callback();
    app.ignore_case();
    app.ignore_underscore();
    app.configurable();
#ifdef _WIN32
    app.allow_windows_style_options(false);
#else
    app.allow_windows_style_options();
#endif

    app.fallthrough();
    app.validate_positionals();
    app.usage("ussy");
    app.footer("footy");
    app.group("Stuff");
    app.require_subcommand(2, 3);

    auto *app2 = app.add_subcommand("app2");

    // Initial defaults
    CHECK(app2->get_allow_extras());
    CHECK(app2->get_prefix_command());
    CHECK(app2->get_immediate_callback());
    CHECK(app2->get_ignore_case());
    CHECK(app2->get_ignore_underscore());
#ifdef _WIN32
    CHECK(!app2->get_allow_windows_style_options());
#else
    CHECK(app2->get_allow_windows_style_options());
#endif
    CHECK(app2->get_fallthrough());
    CHECK(app2->get_validate_positionals());
    CHECK(app2->get_configurable());
    CHECK("ussy" == app2->get_usage());
    CHECK("footy" == app2->get_footer());
    CHECK("Stuff" == app2->get_group());
    CHECK(0u == app2->get_require_subcommand_min());
    CHECK(3u == app2->get_require_subcommand_max());
}

TEST_CASE_METHOD(TApp, "SubcommandMinMax", "[creation]") {

    CHECK(0u == app.get_require_subcommand_min());
    CHECK(0u == app.get_require_subcommand_max());

    app.require_subcommand();

    CHECK(1u == app.get_require_subcommand_min());
    CHECK(0u == app.get_require_subcommand_max());

    app.require_subcommand(2);

    CHECK(2u == app.get_require_subcommand_min());
    CHECK(2u == app.get_require_subcommand_max());

    app.require_subcommand(0);

    CHECK(0u == app.get_require_subcommand_min());
    CHECK(0u == app.get_require_subcommand_max());

    app.require_subcommand(-2);

    CHECK(0u == app.get_require_subcommand_min());
    CHECK(2u == app.get_require_subcommand_max());

    app.require_subcommand(3, 7);

    CHECK(3u == app.get_require_subcommand_min());
    CHECK(7u == app.get_require_subcommand_max());
}

TEST_CASE_METHOD(TApp, "GetOptionList", "[creation]") {
    int two{0};
    auto *flag = app.add_flag("--one");
    auto *opt = app.add_option("--two", two);

    const CLI::App &const_app = app;  // const alias to force use of const-methods
    std::vector<const CLI::Option *> opt_list = const_app.get_options();

    REQUIRE(static_cast<std::size_t>(3) == opt_list.size());
    CHECK(flag == opt_list.at(1));
    CHECK(opt == opt_list.at(2));

    std::vector<CLI::Option *> nonconst_opt_list = app.get_options();
    for(std::size_t i = 0; i < opt_list.size(); ++i) {
        CHECK(opt_list.at(i) == nonconst_opt_list.at(i));
    }
}

TEST_CASE_METHOD(TApp, "GetOptionListFilter", "[creation]") {
    int two{0};
    auto *flag = app.add_flag("--one");
    app.add_option("--two", two);

    const CLI::App &const_app = app;  // const alias to force use of const-methods
    std::vector<const CLI::Option *> opt_listc =
        const_app.get_options([](const CLI::Option *opt) { return opt->get_name() == "--one"; });

    REQUIRE(static_cast<std::size_t>(1) == opt_listc.size());
    CHECK(flag == opt_listc.at(0));

    std::vector<CLI::Option *> opt_list =
        app.get_options([](const CLI::Option *opt) { return opt->get_name() == "--one"; });

    REQUIRE(static_cast<std::size_t>(1) == opt_list.size());
    CHECK(flag == opt_list.at(0));
}

TEST_CASE("ValidatorTests: TestValidatorCreation", "[creation]") {
    std::function<std::string(std::string &)> op1 = [](std::string &val) {
        return (val.size() >= 5) ? std::string{} : val;
    };
    CLI::Validator V(op1, "", "size");

    CHECK("size" == V.get_name());
    V.name("harry");
    CHECK("harry" == V.get_name());
    CHECK(V.get_active());

    CHECK("test" == V("test"));
    CHECK(V("test5").empty());

    CHECK(V.get_description().empty());
    V.description("this is a description");
    CHECK("this is a description" == V.get_description());
}

TEST_CASE("ValidatorTests: TestValidatorOps", "[creation]") {
    std::function<std::string(std::string &)> op1 = [](std::string &val) {
        return (val.size() >= 5) ? std::string{} : val;
    };
    std::function<std::string(std::string &)> op2 = [](std::string &val) {
        return (val.size() >= 9) ? std::string{} : val;
    };
    std::function<std::string(std::string &)> op3 = [](std::string &val) {
        return (val.size() < 3) ? std::string{} : val;
    };
    std::function<std::string(std::string &)> op4 = [](std::string &val) {
        return (val.size() <= 9) ? std::string{} : val;
    };
    CLI::Validator V1(op1, "SIZE >= 5");

    CLI::Validator V2(op2, "SIZE >= 9");
    CLI::Validator V3(op3, "SIZE < 3");
    CLI::Validator V4(op4, "SIZE <= 9");

    std::string two(2, 'a');
    std::string four(4, 'a');
    std::string five(5, 'a');
    std::string eight(8, 'a');
    std::string nine(9, 'a');
    std::string ten(10, 'a');
    CHECK(V1(five).empty());
    CHECK(!V1(four).empty());

    CHECK(V2(nine).empty());
    CHECK(!V2(eight).empty());

    CHECK(V3(two).empty());
    CHECK(!V3(four).empty());

    CHECK(V4(eight).empty());
    CHECK(!V4(ten).empty());

    auto V1a2 = V1 & V2;
    CHECK("(SIZE >= 5) AND (SIZE >= 9)" == V1a2.get_description());
    CHECK(!V1a2(five).empty());
    CHECK(V1a2(nine).empty());

    auto V1a4 = V1 & V4;
    CHECK("(SIZE >= 5) AND (SIZE <= 9)" == V1a4.get_description());
    CHECK(V1a4(five).empty());
    CHECK(V1a4(eight).empty());
    CHECK(!V1a4(ten).empty());
    CHECK(!V1a4(four).empty());

    auto V1o3 = V1 | V3;
    CHECK("(SIZE >= 5) OR (SIZE < 3)" == V1o3.get_description());
    CHECK(V1o3(two).empty());
    CHECK(V1o3(eight).empty());
    CHECK(V1o3(ten).empty());
    CHECK(V1o3(two).empty());
    CHECK(!V1o3(four).empty());

    auto m1 = V1o3 & V4;
    CHECK("((SIZE >= 5) OR (SIZE < 3)) AND (SIZE <= 9)" == m1.get_description());
    CHECK(m1(two).empty());
    CHECK(m1(eight).empty());
    CHECK(!m1(ten).empty());
    CHECK(m1(two).empty());
    CHECK(m1(five).empty());
    CHECK(!m1(four).empty());

    auto m2 = m1 & V2;
    CHECK("(((SIZE >= 5) OR (SIZE < 3)) AND (SIZE <= 9)) AND (SIZE >= 9)" == m2.get_description());
    CHECK(!m2(two).empty());
    CHECK(!m2(eight).empty());
    CHECK(!m2(ten).empty());
    CHECK(!m2(two).empty());
    CHECK(m2(nine).empty());
    CHECK(!m2(four).empty());

    auto m3 = m2 | V3;
    CHECK("((((SIZE >= 5) OR (SIZE < 3)) AND (SIZE <= 9)) AND (SIZE >= 9)) OR (SIZE < 3)" == m3.get_description());
    CHECK(m3(two).empty());
    CHECK(!m3(eight).empty());
    CHECK(m3(nine).empty());
    CHECK(!m3(four).empty());

    auto m4 = V3 | m2;
    CHECK("(SIZE < 3) OR ((((SIZE >= 5) OR (SIZE < 3)) AND (SIZE <= 9)) AND (SIZE >= 9))" == m4.get_description());
    CHECK(m4(two).empty());
    CHECK(!m4(eight).empty());
    CHECK(m4(nine).empty());
    CHECK(!m4(four).empty());
}

TEST_CASE("ValidatorTests: TestValidatorNegation", "[creation]") {

    std::function<std::string(std::string &)> op1 = [](std::string &val) {
        return (val.size() >= 5) ? std::string{} : val;
    };

    CLI::Validator V1(op1, "SIZE >= 5", "size");

    std::string four(4, 'a');
    std::string five(5, 'a');

    CHECK(V1(five).empty());
    CHECK(!V1(four).empty());

    auto V2 = !V1;
    CHECK(!V2(five).empty());
    CHECK(V2(four).empty());
    CHECK("NOT SIZE >= 5" == V2.get_description());

    V2.active(false);
    CHECK(V2(five).empty());
    CHECK(V2(four).empty());
    CHECK(V2.get_description().empty());
}

TEST_CASE("ValidatorTests: ValidatorDefaults", "[creation]") {

    CLI::Validator V1{};

    std::string four(4, 'a');
    std::string five(5, 'a');

    // make sure this doesn't generate a seg fault or something
    CHECK(V1(five).empty());
    CHECK(V1(four).empty());

    CHECK(V1.get_name().empty());
    CHECK(V1.get_description().empty());
    CHECK(V1.get_active());
    CHECK(V1.get_modifying());

    CLI::Validator V2{"check"};
    // make sure this doesn't generate a seg fault or something
    CHECK(V2(five).empty());
    CHECK(V2(four).empty());

    CHECK(V2.get_name().empty());
    CHECK("check" == V2.get_description());
    CHECK(V2.get_active());
    CHECK(V2.get_modifying());
    // This class only support streaming in, not out
}

class Unstreamable {
  private:
    int x_{-1};

  public:
    Unstreamable() = default;
    CLI11_NODISCARD int get_x() const { return x_; }
    void set_x(int x) { x_ = x; }
};

// this needs to be a different check then the one after the function definition otherwise they conflict
static_assert(!CLI::detail::is_istreamable<Unstreamable, std::istream>::value, "Unstreamable type is streamable");

std::istream &operator>>(std::istream &in, Unstreamable &value) {
    int x = 0;
    in >> x;
    value.set_x(x);
    return in;
}
// these need to be different classes otherwise the definitions conflict
static_assert(CLI::detail::is_istreamable<Unstreamable>::value,
              "Unstreamable type is still unstreamable and it should be");

TEST_CASE_METHOD(TApp, "MakeUnstreamableOptions", "[creation]") {
    Unstreamable value;
    app.add_option("--value", value);

    // This used to fail to build, since it tries to stream from Unstreamable
    app.add_option("--value2", value);

    std::vector<Unstreamable> values;
    app.add_option("--values", values);

    // This used to fail to build, since it tries to stream from Unstreamable
    app.add_option("--values2", values);

    args = {"--value", "45"};
    run();
    CHECK(45 == value.get_x());

    args = {"--values", "45", "27", "34"};
    run();
    CHECK(3u == values.size());
    CHECK(34 == values[2].get_x());
}
