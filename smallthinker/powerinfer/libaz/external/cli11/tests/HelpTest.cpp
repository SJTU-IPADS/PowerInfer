// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifdef CLI11_SINGLE_FILE
#include "CLI11.hpp"
#else
#include "CLI/CLI.hpp"
#endif

#include "app_helper.hpp"

#include "catch.hpp"
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("THelp: Basic", "[help]") {
    CLI::App app{"My prog"};

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, Contains("-h,     --help"));
    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("Usage:"));
}

TEST_CASE("THelp: Usage", "[help]") {
    CLI::App app{"My prog"};
    app.usage("use: just use it");

    std::string help = app.help();

    CHECK_THAT(help, Contains("use: just use it"));
}

TEST_CASE("THelp: UsageCallback", "[help]") {
    CLI::App app{"My prog"};
    app.usage([]() { return "use: just use it"; });

    std::string help = app.help();
    CHECK_THAT(help, Contains("use: just use it"));
}

TEST_CASE("THelp: UsageCallbackBoth", "[help]") {
    CLI::App app{"My prog"};
    app.usage([]() { return "use: just use it"; });
    app.usage("like 1, 2, and 3");
    std::string help = app.help();

    CHECK_THAT(help, Contains("use: just use it"));
    CHECK_THAT(help, Contains("like 1, 2, and 3"));
}

TEST_CASE("THelp: Footer", "[help]") {
    CLI::App app{"My prog"};
    app.footer("Report bugs to bugs@example.com");

    std::string help = app.help();

    CHECK_THAT(help, Contains("Report bugs to bugs@example.com"));
}

TEST_CASE("THelp: FooterCallback", "[help]") {
    CLI::App app{"My prog"};
    app.footer([]() { return "Report bugs to bugs@example.com"; });

    std::string help = app.help();

    CHECK_THAT(help, Contains("Report bugs to bugs@example.com"));
}

TEST_CASE("THelp: FooterCallbackBoth", "[help]") {
    CLI::App app{"My prog"};
    app.footer([]() { return "Report bugs to bugs@example.com"; });
    app.footer(" foot!!!!");
    std::string help = app.help();

    CHECK_THAT(help, Contains("Report bugs to bugs@example.com"));
    CHECK_THAT(help, Contains("foot!!!!"));
}

TEST_CASE("THelp: OptionalPositional", "[help]") {
    CLI::App app{"My prog", "program"};

    std::string x;
    app.add_option("something", x, "My option here");

    std::string help = app.help();

    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("POSITIONALS:"));
    CHECK_THAT(help, Contains("something TEXT"));
    CHECK_THAT(help, Contains("My option here"));
    CHECK_THAT(help, Contains("program [OPTIONS] [something]"));
}

TEST_CASE("THelp: Hidden", "[help]") {
    CLI::App app{"My prog"};

    std::string x;
    app.add_option("something", x, "My option here")->group("");
    std::string y;
    app.add_option("--another", y)->group("");

    std::string help = app.help();

    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, !Contains("[something]"));
    CHECK_THAT(help, !Contains("something "));
    CHECK_THAT(help, !Contains("another"));
}

TEST_CASE("THelp: deprecatedOptions", "[help]") {
    CLI::App app{"My prog"};

    std::string x;
    auto *soption = app.add_option("--something", x, "My option here");
    app.add_option("--something_else", x, "My option here");
    std::string y;
    app.add_option("--another", y);

    CLI::deprecate_option(soption, "something_else");

    std::string help = app.help();

    CHECK_THAT(help, Contains("DEPRECATED"));
    CHECK_THAT(help, Contains("something"));
    CHECK_NOTHROW(app.parse("--something deprecated"));
}

TEST_CASE("THelp: deprecatedOptions2", "[help]") {
    CLI::App app{"My prog"};

    std::string x;
    app.add_option("--something", x, "My option here");
    app.add_option("--something_else", x, "My option here");
    std::string y;
    app.add_option("--another", y);

    CLI::deprecate_option(&app, "--something");

    std::string help = app.help();

    CHECK_THAT(help, Contains("DEPRECATED"));
    CHECK_THAT(help, Contains("something"));
    CHECK_NOTHROW(app.parse("--something deprecated"));
}

TEST_CASE("THelp: deprecatedOptions3", "[help]") {
    CLI::App app{"My prog"};
    app.get_formatter()->right_column_width(100);
    std::string x;
    app.add_option("--something", x, "Some Description");
    app.add_option("--something_else", x, "Some other description");
    std::string y;
    app.add_option("--another", y);

    CLI::deprecate_option(app, "--something", "--something_else");

    std::string help = app.help();

    CHECK_THAT(help, Contains("DEPRECATED"));
    CHECK_THAT(help, Contains("'--something_else' instead"));
    CHECK_NOTHROW(app.parse("--something deprecated"));
}

TEST_CASE("THelp: retiredOptions", "[help]") {
    CLI::App app{"My prog"};

    std::string x;
    auto *opt1 = app.add_option("--something", x, "My option here");
    app.add_option("--something_else", x, "My option here");
    std::string y;
    app.add_option("--another", y);

    CLI::retire_option(app, opt1);

    std::string help = app.help();

    CHECK_THAT(help, Contains("RETIRED"));
    CHECK_THAT(help, Contains("something"));

    CHECK_NOTHROW(app.parse("--something old"));
}

TEST_CASE("THelp: retiredOptions2", "[help]") {
    CLI::App app{"My prog"};

    std::string x;
    app.add_option("--something_else", x, "My option here");
    std::string y;
    app.add_option("--another", y);

    CLI::retire_option(&app, "--something");

    std::string help = app.help();

    CHECK_THAT(help, Contains("RETIRED"));
    CHECK_THAT(help, Contains("something"));
    CHECK_NOTHROW(app.parse("--something old"));
}

TEST_CASE("THelp: retiredOptions3", "[help]") {
    CLI::App app{"My prog"};

    std::string x;
    app.add_option("--something", x, "My option here");
    app.add_option("--something_else", x, "My option here");
    std::string y;
    app.add_option("--another", y);

    CLI::retire_option(app, "--something");

    std::string help = app.help();

    CHECK_THAT(help, Contains("RETIRED"));
    CHECK_THAT(help, Contains("something"));

    CHECK_NOTHROW(app.parse("--something old"));
}

TEST_CASE("THelp: HiddenGroup", "[help]") {
    CLI::App app{"My prog"};
    // empty option group name should be hidden
    auto *hgroup = app.add_option_group("");
    std::string x;
    hgroup->add_option("something", x, "My option here");
    std::string y;
    hgroup->add_option("--another", y);

    std::string help = app.help();

    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, !Contains("[something]"));
    CHECK_THAT(help, !Contains("something "));
    CHECK_THAT(help, !Contains("another"));

    hgroup->group("ghidden");

    help = app.help();

    CHECK_THAT(help, Contains("something "));
    CHECK_THAT(help, Contains("another"));
}

// from https://github.com/CLIUtils/CLI11/issues/1045
TEST_CASE("THelp: multiple_group", "[help]") {
    CLI::App app{"test_group"};
    auto *group1 = app.add_option_group("outGroup");
    auto *group2 = app.add_option_group("inGroup");

    std::string outFile("");
    group1->add_option("--outfile,-o", outFile, "specify the file location of the output")->required();

    std::string inFile("");
    group2->add_option("--infile,-i", inFile, "specify the file location of the input")->required();

    auto help = app.help();
    int inCount = 0;
    int outCount = 0;
    auto iFind = help.find("inGroup");
    while(iFind != std::string::npos) {
        ++inCount;
        iFind = help.find("inGroup", iFind + 6);
    }
    auto oFind = help.find("outGroup");
    while(oFind != std::string::npos) {
        ++outCount;
        oFind = help.find("outGroup", oFind + 6);
    }
    CHECK(inCount == 1);
    CHECK(outCount == 1);
}

TEST_CASE("THelp: OptionalPositionalAndOptions", "[help]") {
    CLI::App app{"My prog", "AnotherProgram"};
    app.add_flag("-q,--quick");

    std::string x;
    app.add_option("something", x, "My option here");

    std::string help = app.help();

    CHECK_THAT(help, Contains("AnotherProgram [OPTIONS] [something]"));
}

TEST_CASE("THelp: NonStandardOptions", "[help]") {
    CLI::App app{"My prog", "nonstandard"};
    app.allow_non_standard_option_names();
    app.add_flag("-q,--quick");
    app.add_flag("-slow");
    app.add_option("--fast,-not-slow", "a description of what is");
    std::string x;
    app.add_option("something", x, "My option here");

    std::string help = app.help();

    CHECK_THAT(help, Contains("-not-slow"));
}

TEST_CASE("THelp: RequiredPositionalAndOptions", "[help]") {
    CLI::App app{"My prog"};
    app.add_flag("-q,--quick");

    std::string x;
    app.add_option("something", x, "My option here")->required();

    std::string help = app.help();

    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("POSITIONALS:"));
    CHECK_THAT(help, Contains("Usage: [OPTIONS] something"));
}

TEST_CASE("THelp: MultiOpts", "[help]") {
    CLI::App app{"My prog"};
    std::vector<int> x, y;
    app.add_option("-q,--quick", x, "Disc")->expected(2);
    app.add_option("-v,--vals", y, "Other");

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, !Contains("POSITIONALS:"));
    CHECK_THAT(help, Contains("Usage: [OPTIONS]"));
    CHECK_THAT(help, Contains("INT x 2"));
    CHECK_THAT(help, Contains("INT ..."));
}

TEST_CASE("THelp: VectorOpts", "[help]") {
    CLI::App app{"My prog"};
    std::vector<int> x = {1, 2};
    app.add_option("-q,--quick", x)->capture_default_str();

    std::string help = app.help();

    CHECK_THAT(help, Contains("[1,2]"));
    CHECK_THAT(help, Contains(" ..."));
}

TEST_CASE("THelp: MultiPosOpts", "[help]") {
    CLI::App app{"My prog"};
    app.name("program");
    std::vector<int> x, y;
    app.add_option("quick", x, "Disc")->expected(2);
    app.add_option("vals", y, "Other");

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, Contains("POSITIONALS:"));
    CHECK_THAT(help, Contains("program [OPTIONS]"));
    CHECK_THAT(help, Contains("INT x 2"));
    CHECK_THAT(help, Contains("INT ..."));
    CHECK_THAT(help, Contains("[quick(2x)]"));
    CHECK_THAT(help, Contains("[vals...]"));
}

TEST_CASE("THelp: EnvName", "[help]") {
    CLI::App app{"My prog"};
    std::string input;
    app.add_option("--something", input)->envname("SOME_ENV");

    std::string help = app.help();

    CHECK_THAT(help, Contains("SOME_ENV"));
}

TEST_CASE("THelp: Needs", "[help]") {
    CLI::App app{"My prog"};

    CLI::Option *op1 = app.add_flag("--op1");
    app.add_flag("--op2")->needs(op1);

    std::string help = app.help();

    CHECK_THAT(help, Contains("Needs: --op1"));
}

TEST_CASE("THelp: NeedsPositional", "[help]") {
    CLI::App app{"My prog"};

    int x{0}, y{0};

    CLI::Option *op1 = app.add_option("op1", x, "one");
    app.add_option("op2", y, "two")->needs(op1);

    std::string help = app.help();

    CHECK_THAT(help, Contains("POSITIONALS:"));
    CHECK_THAT(help, Contains("Needs: op1"));
}

TEST_CASE("THelp: Excludes", "[help]") {
    CLI::App app{"My prog"};

    CLI::Option *op1 = app.add_flag("--op1");
    app.add_flag("--op2")->excludes(op1);

    std::string help = app.help();

    CHECK_THAT(help, Contains("Excludes: --op1"));
}

TEST_CASE("THelp: ExcludesPositional", "[help]") {
    CLI::App app{"My prog"};

    int x{0}, y{0};

    CLI::Option *op1 = app.add_option("op1", x);
    app.add_option("op2", y)->excludes(op1);

    std::string help = app.help();

    CHECK_THAT(help, Contains("POSITIONALS:"));
    CHECK_THAT(help, Contains("Excludes: op1"));
}

TEST_CASE("THelp: ExcludesSymmetric", "[help]") {
    CLI::App app{"My prog"};

    CLI::Option *op1 = app.add_flag("--op1");
    app.add_flag("--op2")->excludes(op1);

    std::string help = app.help();

    CHECK_THAT(help, Contains("Excludes: --op2"));
}

TEST_CASE("THelp: ManualSetters", "[help]") {

    CLI::App app{"My prog"};

    int x{1};

    CLI::Option *op1 = app.add_option("--op", x);
    op1->default_str("12");
    op1->type_name("BIGGLES");
    CHECK(1 == x);

    std::string help = app.help();

    CHECK_THAT(help, Contains("[12]"));
    CHECK_THAT(help, Contains("BIGGLES"));

    op1->default_val("14");
    CHECK(14 == x);
    help = app.help();
    CHECK_THAT(help, Contains("[14]"));

    op1->default_val(12);
    CHECK(12 == x);
    help = app.help();
    CHECK_THAT(help, Contains("[12]"));

    CHECK(op1->get_run_callback_for_default());
    op1->run_callback_for_default(false);
    CHECK(!op1->get_run_callback_for_default());

    op1->default_val(18);
    // x should not be modified in this case
    CHECK(12 == x);
    help = app.help();
    CHECK_THAT(help, Contains("[18]"));
}

TEST_CASE("THelp: ManualSetterOverFunction", "[help]") {

    CLI::App app{"My prog"};

    int x{1};

    CLI::Option *op1 = app.add_option("--op1", x)->check(CLI::IsMember({1, 2}));
    CLI::Option *op2 = app.add_option("--op2", x)->transform(CLI::IsMember({1, 2}));
    op1->default_str("12");
    op1->type_name("BIGGLES");
    op2->type_name("QUIGGLES");
    CHECK(1 == x);

    std::string help = app.help();
    CHECK_THAT(help, Contains("[12]"));
    CHECK_THAT(help, Contains("BIGGLES"));
    CHECK_THAT(help, Contains("QUIGGLES"));
    CHECK_THAT(help, Contains("{1,2}"));
}

TEST_CASE("THelp: Subcom", "[help]") {
    CLI::App app{"My prog"};

    auto *sub1 = app.add_subcommand("sub1");
    app.add_subcommand("sub2");

    std::string help = app.help();
    CHECK_THAT(help, Contains("Usage: [OPTIONS] [SUBCOMMAND]"));

    app.require_subcommand();

    help = app.help();
    CHECK_THAT(help, Contains("Usage: [OPTIONS] SUBCOMMAND"));

    help = sub1->help();
    CHECK_THAT(help, Contains("sub1 [OPTIONS]"));

    char x[] = "./myprogram";  // NOLINT(modernize-avoid-c-arrays)
    char y[] = "sub2";         // NOLINT(modernize-avoid-c-arrays)

    std::vector<char *> args = {x, y};
    app.parse(static_cast<int>(args.size()), args.data());

    help = app.help();
    CHECK_THAT(help, Contains("./myprogram sub2"));
}

TEST_CASE("THelp: Subcom_alias", "[help]") {
    CLI::App app{"My prog"};

    auto *sub1 = app.add_subcommand("sub1", "Subcommand1 description test");
    sub1->alias("sub_alias1");
    sub1->alias("sub_alias2");

    app.add_subcommand("sub2", "Subcommand2 description test");

    std::string help = app.help();
    CHECK_THAT(help, Contains("Usage: [OPTIONS] [SUBCOMMAND]"));
    CHECK_THAT(help, Contains("sub_alias1"));
    CHECK_THAT(help, Contains("sub_alias2"));
}

TEST_CASE("THelp: Subcom_alias_group", "[help]") {
    CLI::App app{"My prog"};

    auto *sub1 = app.add_subcommand("", "Subcommand1 description test");
    sub1->alias("sub_alias1");
    sub1->alias("sub_alias2");

    app.add_subcommand("sub2", "Subcommand2 description test");

    std::string help = app.help();
    CHECK_THAT(help, Contains("Usage: [OPTIONS] [SUBCOMMAND]"));
    CHECK_THAT(help, Contains("sub_alias1"));
    CHECK_THAT(help, Contains("sub_alias2"));
}

TEST_CASE("THelp: MasterName", "[help]") {
    CLI::App app{"My prog", "MyRealName"};

    char x[] = "./myprogram";  // NOLINT(modernize-avoid-c-arrays)

    std::vector<char *> args = {x};
    app.parse(static_cast<int>(args.size()), args.data());

    CHECK_THAT(app.help(), Contains("MyRealName"));
}

TEST_CASE("THelp: IntDefaults", "[help]") {
    CLI::App app{"My prog"};

    int one{1}, two{2};
    app.add_option("--one", one, "Help for one")->capture_default_str();
    app.add_option("--set", two, "Help for set")->capture_default_str()->check(CLI::IsMember({2, 3, 4}));

    std::string help = app.help();

    CHECK_THAT(help, Contains("--one"));
    CHECK_THAT(help, Contains("--set"));
    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, Contains("[2]"));
    CHECK_THAT(help, Contains("2,3,4"));
}

TEST_CASE("THelp: SetLower", "[help]") {
    CLI::App app{"My prog"};
    app.option_defaults()->always_capture_default();

    std::string def{"One"};
    app.add_option("--set", def, "Help for set")->check(CLI::IsMember({"oNe", "twO", "THREE"}));

    std::string help = app.help();

    CHECK_THAT(help, Contains("--set"));
    CHECK_THAT(help, Contains("[One]"));
    CHECK_THAT(help, Contains("oNe"));
    CHECK_THAT(help, Contains("twO"));
    CHECK_THAT(help, Contains("THREE"));
}

TEST_CASE("THelp: OnlyOneHelp", "[help]") {
    CLI::App app{"My prog"};

    // It is not supported to have more than one help flag, last one wins
    app.set_help_flag("--help", "No short name allowed");
    app.set_help_flag("--yelp", "Alias for help");

    std::vector<std::string> input{"--help"};
    CHECK_THROWS_AS(app.parse(input), CLI::ExtrasError);
}

TEST_CASE("THelp: MultiHelp", "[help]") {
    CLI::App app{"My prog"};

    // It is not supported to have more than one help flag, last one wins
    app.set_help_flag("--help,-h,-?", "No short name allowed");
    app.allow_windows_style_options();

    std::vector<std::string> input{"/?"};
    CHECK_THROWS_AS(app.parse(input), CLI::CallForHelp);
}

TEST_CASE("THelp: OnlyOneAllHelp", "[help]") {
    CLI::App app{"My prog"};

    // It is not supported to have more than one help flag, last one wins
    app.set_help_all_flag("--help-all", "No short name allowed");
    app.set_help_all_flag("--yelp", "Alias for help");

    std::vector<std::string> input{"--help-all"};
    CHECK_THROWS_AS(app.parse(input), CLI::ExtrasError);

    std::vector<std::string> input2{"--yelp"};
    CHECK_THROWS_AS(app.parse(input2), CLI::CallForAllHelp);

    // Remove the flag
    app.set_help_all_flag();
    std::vector<std::string> input3{"--yelp"};
    CHECK_THROWS_AS(app.parse(input3), CLI::ExtrasError);
}

TEST_CASE("THelp: RemoveHelp", "[help]") {
    CLI::App app{"My prog"};
    app.set_help_flag();

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, !Contains("-h,--help"));
    CHECK_THAT(help, !Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("Usage:"));

    std::vector<std::string> input{"--help"};
    try {
        app.parse(input);
    } catch(const CLI::ParseError &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::ExtrasError));
    }
}

TEST_CASE("THelp: RemoveOtherMethodHelp", "[help]") {
    CLI::App app{"My prog"};

    // Don't do this. Just in case, let's make sure it works.
    app.remove_option(const_cast<CLI::Option *>(app.get_help_ptr()));

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, !Contains("-h,--help"));
    CHECK_THAT(help, !Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("Usage:"));

    std::vector<std::string> input{"--help"};
    try {
        app.parse(input);
    } catch(const CLI::ParseError &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::ExtrasError));
    }
}

TEST_CASE("THelp: RemoveOtherMethodHelpAll", "[help]") {
    CLI::App app{"My prog"};

    app.set_help_all_flag("--help-all");
    // Don't do this. Just in case, let's make sure it works.
    app.remove_option(const_cast<CLI::Option *>(app.get_help_all_ptr()));

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, !Contains("--help-all"));
    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("Usage:"));

    std::vector<std::string> input{"--help-all"};
    try {
        app.parse(input);
    } catch(const CLI::ParseError &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::ExtrasError));
    }
}

TEST_CASE("THelp: NoHelp", "[help]") {
    CLI::App app{"My prog"};
    app.set_help_flag();

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, !Contains("-h,--help"));
    CHECK_THAT(help, !Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("Usage:"));

    std::vector<std::string> input{"--help"};
    try {
        app.parse(input);
    } catch(const CLI::ParseError &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::ExtrasError));
    }
}

TEST_CASE("THelp: CustomHelp", "[help]") {
    CLI::App app{"My prog"};

    CLI::Option *help_option = app.set_help_flag("--yelp", "display help and exit");
    CHECK(help_option == app.get_help_ptr());

    std::string help = app.help();

    CHECK_THAT(help, Contains("My prog"));
    CHECK_THAT(help, !Contains("-h,--help"));
    CHECK_THAT(help, Contains("--yelp"));
    CHECK_THAT(help, Contains("OPTIONS:"));
    CHECK_THAT(help, Contains("Usage:"));

    std::vector<std::string> input{"--yelp"};
    try {
        app.parse(input);
    } catch(const CLI::CallForHelp &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::Success));
    }
}

TEST_CASE("THelp: HelpSubcommandPriority", "[help]") {
    CLI::App app{"My prog"};

    app.set_help_flag("-h", "display help and exit");

    auto *sub1 = app.add_subcommand("sub1");
    std::string someFile = "";

    put_env("SOME_FILE", "NOT_A_FILE");
    sub1->add_option("-f,--file", someFile)->envname("SOME_FILE")->required()->expected(1)->check(CLI::ExistingFile);

    std::string input{"sub1 -h"};
    CHECK_THROWS_AS(app.parse(input), CLI::CallForHelp);
    unset_env("SOME_FILE");
}

TEST_CASE("THelp: NextLineShouldBeAlignmentInMultilineDescription", "[help]") {
    CLI::App app;
    int i{0};
    const std::string first{"first line"};
    const std::string second{"second line"};
    app.add_option("-i,--int", i, first + "\n" + second);

    const std::string help = app.help();
    const auto width = app.get_formatter()->get_column_width();
    auto first_loc = help.find("first");
    auto first_new_line = help.find_last_of('\n', first_loc);
    auto second_loc = help.find("second");
    auto second_new_line = help.find_last_of('\n', second_loc);
    CHECK(first_loc - first_new_line - 1 == width);
    CHECK(second_loc - second_new_line - 1 == width);
    CHECK(second_new_line > first_loc);
}

TEST_CASE("THelp: CheckRightWidth", "[help]") {
    CLI::App app;
    int i{0};
    const std::string first{"first line"};
    const std::string second{"second line"};
    app.add_option("-i,--int", i, first + "\n" + second);
    app.get_formatter()->column_width(24);
    CHECK(app.get_formatter()->get_column_width() == 24);
    const std::string help = app.help();
    auto first_loc = help.find("first");
    auto first_new_line = help.find_last_of('\n', first_loc);
    auto second_loc = help.find("second");
    auto second_new_line = help.find_last_of('\n', second_loc);
    CHECK(first_loc - first_new_line - 1 == 24);
    CHECK(second_loc - second_new_line - 1 == 24);
    CHECK(second_new_line > first_loc);
}

static const std::string long_string{
    "AAARG this is a long line description that will span across multiple lines and still go on and on.  This is meant "
    "to test how the help handler handles things like this"};

TEST_CASE("THelp: longLineAlignment", "[help]") {
    CLI::App app;
    int i{0};

    app.add_option("-i,--int,--int_very_long_option_name_that_just_keeps_going_on_and_on_and_on_and_on_and_on_possibly_"
                   "to_infinity,--and_another_long_name_just_for_fun",
                   i,
                   long_string);

    std::string help = app.help();
    auto width = app.get_formatter()->get_right_column_width();
    auto first_loc = help.find("AAARG");
    auto first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
    app.get_formatter()->right_column_width(30);
    width = app.get_formatter()->get_right_column_width();
    CHECK(width == 30);
    help = app.help();
    first_loc = help.find("AAARG");
    first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
}

TEST_CASE("THelp: longPositional", "[help]") {
    CLI::App app;
    int i{0};

    app.add_option("int_very_long_option_name_that_just_keeps_going_on_and_on_and_on_and_on_and_on_possibly_"
                   "to_infinity",
                   i,
                   long_string);

    std::string help = app.help();
    auto width = app.get_formatter()->get_right_column_width();
    auto first_loc = help.find("AAARG");
    auto first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
}

TEST_CASE("THelp: SubcommandNewLineDescription", "[help]") {

    const std::string nl_description{"this is a description with aX \n X\\n in it and just for fun \n\t another"};

    CLI::App app;
    int i{0};
    app.add_option("-i,--int", i);
    app.add_subcommand("subcom1", nl_description);
    std::string help = app.help();
    auto width = app.get_formatter()->get_column_width();
    auto first_X = help.find_first_of('X');
    auto first_new_line = help.find_first_of('\n', first_X);
    auto second_X = help.find_first_of('X', first_new_line);
    CHECK(second_X - first_new_line > width);
}

TEST_CASE("THelp: longDescription", "[help]") {

    CLI::App app(long_string, "long_desc");
    int i{0};

    app.add_option("-i,--int", i);

    std::string help = app.help();
    auto width = app.get_formatter()->get_description_paragraph_width();
    auto first_loc = help.find("AAARG");
    auto first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
    app.get_formatter()->description_paragraph_width(30);
    width = app.get_formatter()->get_description_paragraph_width();
    CHECK(width == 30);
    help = app.help();
    first_loc = help.find("AAARG");
    first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
}

TEST_CASE("THelp: longSubcommandDescription", "[help]") {

    CLI::App app;
    int i{0};

    app.add_option("-i,--int", i);
    app.add_subcommand("test1", long_string);
    std::string help = app.help();
    auto width = app.get_formatter()->get_right_column_width();
    auto first_loc = help.find("AAARG");
    auto first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
    app.get_formatter()->right_column_width(30);
    width = 30;
    help = app.help();
    first_loc = help.find("AAARG");
    first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
}

TEST_CASE("THelp: longSubcommandDescriptionExpanded", "[help]") {

    CLI::App app;
    int i{0};

    app.add_option("-i,--int", i);
    app.add_subcommand("test1", long_string);

    auto help = app.help("", CLI::AppFormatMode::All);
    auto width = app.get_formatter()->get_description_paragraph_width();
    auto first_loc = help.find("AAARG");
    auto first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
}

TEST_CASE("THelp: longFooter", "[help]") {
    CLI::App app("test long footer", "long_desc");
    int i{0};
    app.footer(long_string);
    app.add_option("-i,--int", i);

    std::string help = app.help();
    auto width = app.get_formatter()->get_footer_paragraph_width();
    auto first_loc = help.find("AAARG");
    auto first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
    app.get_formatter()->footer_paragraph_width(30);
    width = app.get_formatter()->get_footer_paragraph_width();
    CHECK(width == 30);
    help = app.help();
    first_loc = help.find("AAARG");
    first_new_line = help.find_first_of('\n', first_loc);

    CHECK(first_new_line - first_loc - 1 < width);
}

TEST_CASE("THelp: NiceName", "[help]") {
    CLI::App app;

    int x{0};
    auto *long_name = app.add_option("-s,--long,-q,--other,that", x);
    auto *short_name = app.add_option("more,-x,-y", x);
    auto *positional = app.add_option("posit", x);

    CHECK("--long" == long_name->get_name());
    CHECK("-x" == short_name->get_name());
    CHECK("posit" == positional->get_name());
}

TEST_CASE("Exit: ErrorWithHelp", "[help]") {
    CLI::App app{"My prog"};

    std::vector<std::string> input{"-h"};
    try {
        app.parse(input);
    } catch(const CLI::CallForHelp &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::Success));
    }
}

TEST_CASE("Exit: ErrorWithAllHelp", "[help]") {
    CLI::App app{"My prog"};
    app.set_help_all_flag("--help-all", "All help");

    std::vector<std::string> input{"--help-all"};
    try {
        app.parse(input);
    } catch(const CLI::CallForAllHelp &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::Success));
    }
}

TEST_CASE("Exit: ErrorWithoutHelp", "[help]") {
    CLI::App app{"My prog"};

    std::vector<std::string> input{"--none"};
    try {
        app.parse(input);
    } catch(const CLI::ParseError &e) {
        CHECK(e.get_exit_code() == static_cast<int>(CLI::ExitCodes::ExtrasError));
    }
}

TEST_CASE("Exit: ExitCodes", "[help]") {
    CLI::App app;

    auto i = static_cast<int>(CLI::ExitCodes::ExtrasError);
    CHECK(app.exit(CLI::Success()) == 0);
    CHECK(app.exit(CLI::CallForHelp()) == 0);
    CHECK(app.exit(CLI::ExtrasError({"Thing"})) == i);
    CHECK(app.exit(CLI::RuntimeError(42)) == 42);
    CHECK(app.exit(CLI::RuntimeError()) == 1);
}

struct CapturedHelp {
    CLI::App app{"My Test Program"};
    std::stringstream out{};
    std::stringstream err{};

    int run(const CLI::Error &e) { return app.exit(e, out, err); }

    void reset() {
        out.clear();
        err.clear();
    }
};

TEST_CASE_METHOD(CapturedHelp, "Successful", "[help]") {
    CHECK(0 == run(CLI::Success()));
    CHECK(out.str().empty());
    CHECK(err.str().empty());
}

TEST_CASE_METHOD(CapturedHelp, "JustAnError", "[help]") {
    CHECK(42 == run(CLI::RuntimeError(42)));
    CHECK(out.str().empty());
    CHECK(err.str().empty());
}

TEST_CASE_METHOD(CapturedHelp, "CallForHelp", "[help]") {
    CHECK(0 == run(CLI::CallForHelp()));
    CHECK(app.help() == out.str());
    CHECK(err.str().empty());
}
TEST_CASE_METHOD(CapturedHelp, "CallForAllHelp", "[help]") {
    CHECK(0 == run(CLI::CallForAllHelp()));
    CHECK(app.help("", CLI::AppFormatMode::All) == out.str());
    CHECK(err.str().empty());
}
TEST_CASE_METHOD(CapturedHelp, "CallForAllHelpOutput", "[help]") {
    app.set_help_all_flag("--help-all", "Help all");
    app.add_subcommand("one", "One description");
    CLI::App *sub = app.add_subcommand("two");
    sub->add_flag("--three");

    CHECK(0 == run(CLI::CallForAllHelp()));
    CHECK(app.help("", CLI::AppFormatMode::All) == out.str());
    CHECK(err.str().empty());
    CHECK_THAT(out.str(), Contains("one"));
    CHECK_THAT(out.str(), Contains("two"));
    CHECK_THAT(out.str(), Contains("--three"));
    CHECK_THAT(out.str(), Contains("SUBCOMMANDS:"));
    CHECK_THAT(out.str(), Contains("--help-all"));
    CHECK_THAT(out.str(), Contains("My Test Program"));
}
TEST_CASE_METHOD(CapturedHelp, "NewFormattedHelp", "[help]") {
    app.formatter_fn([](const CLI::App *, std::string, CLI::AppFormatMode) { return "New Help"; });
    CHECK(0 == run(CLI::CallForHelp()));
    CHECK("New Help" == out.str());
    CHECK(err.str().empty());
}

TEST_CASE_METHOD(CapturedHelp, "NormalError", "[help]") {
    CHECK(static_cast<int>(CLI::ExitCodes::ExtrasError) == run(CLI::ExtrasError({"Thing"})));
    CHECK(out.str().empty());
    CHECK_THAT(err.str(), Contains("for more information"));
    CHECK_THAT(err.str(), !Contains("ExtrasError"));
    CHECK_THAT(err.str(), Contains("Thing"));
    CHECK_THAT(err.str(), !Contains(" or "));
    CHECK_THAT(err.str(), !Contains("Usage"));
}

TEST_CASE_METHOD(CapturedHelp, "DoubleError", "[help]") {
    app.set_help_all_flag("--help-all");
    CHECK(static_cast<int>(CLI::ExitCodes::ExtrasError) == run(CLI::ExtrasError({"Thing"})));
    CHECK(out.str().empty());
    CHECK_THAT(err.str(), Contains("for more information"));
    CHECK_THAT(err.str(), Contains(" --help "));
    CHECK_THAT(err.str(), Contains(" --help-all "));
    CHECK_THAT(err.str(), Contains(" or "));
    CHECK_THAT(err.str(), !Contains("ExtrasError"));
    CHECK_THAT(err.str(), Contains("Thing"));
    CHECK_THAT(err.str(), !Contains("Usage"));
}

TEST_CASE_METHOD(CapturedHelp, "AllOnlyError", "[help]") {
    app.set_help_all_flag("--help-all");
    app.set_help_flag();
    CHECK(static_cast<int>(CLI::ExitCodes::ExtrasError) == run(CLI::ExtrasError({"Thing"})));
    CHECK(out.str().empty());
    CHECK_THAT(err.str(), Contains("for more information"));
    CHECK_THAT(err.str(), !Contains(" --help "));
    CHECK_THAT(err.str(), Contains(" --help-all "));
    CHECK_THAT(err.str(), !Contains(" or "));
    CHECK_THAT(err.str(), !Contains("ExtrasError"));
    CHECK_THAT(err.str(), Contains("Thing"));
    CHECK_THAT(err.str(), !Contains("Usage"));
}

TEST_CASE_METHOD(CapturedHelp, "ReplacedError", "[help]") {
    app.failure_message(CLI::FailureMessage::help);

    CHECK(static_cast<int>(CLI::ExitCodes::ExtrasError) == run(CLI::ExtrasError({"Thing"})));
    CHECK(out.str().empty());
    CHECK_THAT(err.str(), !Contains("for more information"));
    CHECK_THAT(err.str(), Contains("ERROR: ExtrasError"));
    CHECK_THAT(err.str(), Contains("Thing"));
    CHECK_THAT(err.str(), Contains("Usage"));
}

// #87
TEST_CASE("THelp: CustomDoubleOption", "[help]") {

    std::pair<int, double> custom_opt;

    CLI::App app;

    auto *opt = app.add_option("posit", [&custom_opt](CLI::results_t vals) {
        custom_opt = {stol(vals.at(0)), stod(vals.at(1))};
        return true;
    });
    opt->type_name("INT FLOAT")->type_size(2);

    CHECK_THAT(app.help(), !Contains("x 2"));
}

TEST_CASE("THelp: CheckEmptyTypeName", "[help]") {
    CLI::App app;

    auto *opt = app.add_flag("-f,--flag");
    std::string name = opt->get_type_name();
    CHECK(name.empty());
}

TEST_CASE("THelp: FlagDefaults", "[help]") {
    CLI::App app;

    app.add_flag("-t,--not{false}")->default_str("false");
    auto str = app.help();
    CHECK_THAT(str, Contains("--not{false}"));
}

TEST_CASE("THelp: AccessDescription", "[help]") {
    CLI::App app{"My description goes here"};

    CHECK("My description goes here" == app.get_description());
}

TEST_CASE("THelp: SetDescriptionAfterCreation", "[help]") {
    CLI::App app{""};

    app.description("My description goes here");

    CHECK("My description goes here" == app.get_description());
    CHECK_THAT(app.help(), Contains("My description goes here"));
}

TEST_CASE("THelp: AccessOptionDescription", "[help]") {
    CLI::App app{};

    int x{0};
    auto *opt = app.add_option("-a,--alpha", x, "My description goes here");

    CHECK("My description goes here" == opt->get_description());
}

TEST_CASE("THelp: SetOptionDescriptionAfterCreation", "[help]") {
    CLI::App app{};

    int x{0};
    auto *opt = app.add_option("-a,--alpha", x);
    opt->description("My description goes here");

    CHECK("My description goes here" == opt->get_description());
    CHECK_THAT(app.help(), Contains("My description goes here"));
}

TEST_CASE("THelp: CleanNeeds", "[help]") {
    CLI::App app;

    int x{0};
    auto *a_name = app.add_option("-a,--alpha", x);
    app.add_option("-b,--boo", x)->needs(a_name);

    CHECK_THAT(app.help(), !Contains("Requires"));
    CHECK_THAT(app.help(), !Contains("Needs: -a,--alpha"));
    CHECK_THAT(app.help(), Contains("Needs: --alpha"));
}

TEST_CASE("THelp: RequiredPrintout", "[help]") {
    CLI::App app;

    int x{0};
    app.add_option("-a,--alpha", x)->required();

    CHECK_THAT(app.help(), Contains(" REQUIRED"));
}

TEST_CASE("THelp: GroupOrder", "[help]") {
    CLI::App app;

    app.add_flag("--one")->group("zee");
    app.add_flag("--two")->group("aee");

    std::string help = app.help();

    auto zee_loc = help.find("zee");
    auto aee_loc = help.find("aee");

    CHECK(std::string::npos != zee_loc);
    CHECK(std::string::npos != aee_loc);
    CHECK(aee_loc > zee_loc);
}

TEST_CASE("THelp: GroupNameError", "[help]") {
    CLI::App app;

    auto *f1 = app.add_flag("--one");
    auto *f2 = app.add_flag("--two");

    CHECK_THROWS_AS(f1->group("evil group name\non two lines"), CLI::IncorrectConstruction);
    CHECK_THROWS_AS(f2->group(std::string(5, '\0')), CLI::IncorrectConstruction);
}

TEST_CASE("THelp: ValidatorsText", "[help]") {
    CLI::App app;

    std::string filename;
    int x{0};
    unsigned int y{0};
    app.add_option("--f1", filename)->check(CLI::ExistingFile);
    app.add_option("--f3", x)->check(CLI::Range(1, 4));
    app.add_option("--f4", y)->check(CLI::Range(12));

    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:FILE"));
    CHECK_THAT(help, Contains("INT in [1 - 4]"));
    CHECK_THAT(help, Contains("UINT:INT in [0 - 12]"));
}

TEST_CASE("THelp: ValidatorsTextCustom", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f1", filename)->check(CLI::ExistingFile.description("Existing file"));

    std::string help = app.help();
    CHECK_THAT(help, Contains("Existing file"));
}

TEST_CASE("THelp: ValidatorsNonPathText", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f2", filename)->check(CLI::NonexistentPath);

    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:PATH"));
}

TEST_CASE("THelp: ValidatorsDirText", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f2", filename)->check(CLI::ExistingDirectory);

    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:DIR"));
}

TEST_CASE("THelp: ValidatorsPathText", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f2", filename)->check(CLI::ExistingPath);

    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:PATH"));
}

TEST_CASE("THelp: CombinedValidatorsText", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f1", filename)->check(CLI::ExistingFile | CLI::ExistingDirectory);

    // This would be nice if it put something other than string, but would it be path or file?
    // Can't programmatically tell!
    // (Users can use ExistingPath, by the way)
    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:(FILE) OR (DIR)"));
    CHECK_THAT(help, !Contains("PATH"));
}

// Don't do this in real life, please
TEST_CASE("THelp: CombinedValidatorsPathyText", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f1", filename)->check(CLI::ExistingPath | CLI::NonexistentPath);

    // Combining validators with the same type string is OK
    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:"));
    CHECK_THAT(help, Contains("PATH"));
}

// Don't do this in real life, please (and transform does nothing here)
TEST_CASE("THelp: CombinedValidatorsPathyTextAsTransform", "[help]") {
    CLI::App app;

    std::string filename;
    app.add_option("--f1", filename)->transform(CLI::ExistingPath | CLI::NonexistentPath);

    // Combining validators with the same type string is OK
    std::string help = app.help();
    CHECK_THAT(help, Contains("TEXT:(PATH(existing)) OR (PATH"));
}

// #113 Part 2
TEST_CASE("THelp: ChangingSet", "[help]") {
    CLI::App app;

    std::set<int> vals{1, 2, 3};
    int val{0};
    app.add_option("--val", val)->check(CLI::IsMember(&vals));

    std::string help = app.help();

    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, !Contains("4"));

    vals.insert(4);
    vals.erase(1);

    help = app.help();

    CHECK_THAT(help, !Contains("1"));
    CHECK_THAT(help, Contains("4"));
}

TEST_CASE("THelp: ChangingSetDefaulted", "[help]") {
    CLI::App app;

    std::set<int> vals{1, 2, 3};
    int val{2};
    app.add_option("--val", val, "")->check(CLI::IsMember(&vals))->capture_default_str();

    std::string help = app.help();

    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, !Contains("4"));

    vals.insert(4);
    vals.erase(1);

    help = app.help();

    CHECK_THAT(help, !Contains("1"));
    CHECK_THAT(help, Contains("4"));
}

TEST_CASE("THelp: ChangingCaselessSet", "[help]") {
    CLI::App app;

    std::set<std::string> vals{"1", "2", "3"};
    std::string val;
    app.add_option("--val", val)->check(CLI::IsMember(&vals, CLI::ignore_case));

    std::string help = app.help();

    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, !Contains("4"));

    vals.insert("4");
    vals.erase("1");

    help = app.help();

    CHECK_THAT(help, !Contains("1"));
    CHECK_THAT(help, Contains("4"));
}

TEST_CASE("THelp: ChangingCaselessSetDefaulted", "[help]") {
    CLI::App app;
    app.option_defaults()->always_capture_default();

    std::set<std::string> vals{"1", "2", "3"};
    std::string val = "2";
    app.add_option("--val", val)->check(CLI::IsMember(&vals, CLI::ignore_case));

    std::string help = app.help();

    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, !Contains("4"));

    vals.insert("4");
    vals.erase("1");

    help = app.help();

    CHECK_THAT(help, !Contains("1"));
    CHECK_THAT(help, Contains("4"));
}

// New defaults tests (1.8)

TEST_CASE("THelp: ChangingDefaults", "[help]") {

    CLI::App app;

    std::vector<int> x = {1, 2};
    CLI::Option *opt = app.add_option("-q,--quick", x);
    x = {3, 4};
    CHECK(x[0] == 3);

    opt->capture_default_str();

    x = {5, 6};
    std::string help = app.help();

    CHECK_THAT(help, Contains("[[3,4]]"));
    CHECK_THAT(help, Contains("..."));
    CHECK_THAT(help, Contains("INT"));
    CHECK(x[0] == 5);
}

TEST_CASE("THelp: ChangingDefaultsWithAutoCapture", "[help]") {

    CLI::App app;
    app.option_defaults()->always_capture_default();

    std::vector<int> x = {1, 2};
    CHECK(x[0] == 1);
    app.add_option("-q,--quick", x);
    x = {3, 4};
    CHECK(x[0] == 3);

    std::string help = app.help();

    CHECK_THAT(help, Contains("[[1,2]]"));
    CHECK_THAT(help, Contains("..."));
}

TEST_CASE("THelp: FunctionDefaultString", "[help]") {

    CLI::App app;

    std::vector<int> x = {1, 2};
    CLI::Option *opt = app.add_option("-q,--quick", x);

    opt->default_function([]() { return std::string("Powerful"); });
    opt->capture_default_str();

    std::string help = app.help();

    CHECK_THAT(help, Contains("[Powerful]"));
}

TEST_CASE("TVersion: simple_flag", "[help]") {

    CLI::App app;

    app.set_version_flag("-v,--version", "VERSION " CLI11_VERSION);

    auto vers = app.version();
    CHECK_THAT(vers, Contains("VERSION"));

    app.set_version_flag();
    CHECK(app.version().empty());
}

TEST_CASE("TVersion: callback_flag", "[help]") {

    CLI::App app;

    app.set_version_flag("-v,--version", []() { return std::string("VERSION " CLI11_VERSION); });

    auto vers = app.version();
    CHECK_THAT(vers, Contains("VERSION"));

    app.set_version_flag("-v", []() { return std::string("VERSION2 " CLI11_VERSION); });
    vers = app.version();
    CHECK_THAT(vers, Contains("VERSION"));
}

TEST_CASE("TVersion: help", "[help]") {

    CLI::App app;

    app.set_version_flag("-v,--version", "version_string", "help_for_version");

    auto hvers = app.help();
    CHECK_THAT(hvers, Contains("help_for_version"));

    app.set_version_flag("-v", []() { return std::string("VERSION2 " CLI11_VERSION); }, "help_for_version2");
    hvers = app.help();
    CHECK_THAT(hvers, Contains("help_for_version2"));
}

TEST_CASE("TVersion: parse_throw", "[help]") {

    CLI::App app;

    app.set_version_flag("--version", CLI11_VERSION);

    CHECK_THROWS_AS(app.parse("--version"), CLI::CallForVersion);
    CHECK_THROWS_AS(app.parse("--version --arg2 5"), CLI::CallForVersion);

    auto *ptr = app.get_version_ptr();

    ptr->ignore_case();
    try {
        app.parse("--Version");
    } catch(const CLI::CallForVersion &v) {
        CHECK_THAT(CLI11_VERSION, Equals(v.what()));
        CHECK(0 == v.get_exit_code());
        const auto &appc = app;
        const auto *cptr = appc.get_version_ptr();
        CHECK(1U == cptr->count());
    }
}

TEST_CASE("TVersion: exit", "[help]") {

    CLI::App app;

    app.set_version_flag("--version", CLI11_VERSION);

    try {
        app.parse("--version");
    } catch(const CLI::CallForVersion &v) {
        std::ostringstream out;
        auto ret = app.exit(v, out);
        CHECK_THAT(out.str(), Contains(CLI11_VERSION));
        CHECK(0 == ret);
    }
}
