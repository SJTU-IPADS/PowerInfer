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

#include "catch.hpp"
#include <fstream>
#include <memory>
#include <string>

class SimpleFormatter : public CLI::FormatterBase {
  public:
    SimpleFormatter() : FormatterBase() {}

    std::string make_help(const CLI::App *, std::string, CLI::AppFormatMode) const override {
        return "This is really simple";
    }
};

TEST_CASE("Formatter: Nothing", "[formatter]") {
    CLI::App app{"My prog"};

    app.formatter(std::make_shared<SimpleFormatter>());

    std::string help = app.help();

    CHECK("This is really simple" == help);
}

TEST_CASE("Formatter: NothingLambda", "[formatter]") {
    CLI::App app{"My prog"};

    app.formatter_fn(
        [](const CLI::App *, std::string, CLI::AppFormatMode) { return std::string("This is really simple"); });

    std::string help = app.help();

    CHECK("This is really simple" == help);
}

TEST_CASE("Formatter: OptCustomize", "[formatter]") {
    CLI::App app{"My prog"};

    auto optfmt = std::make_shared<CLI::Formatter>();
    optfmt->column_width(25);
    optfmt->label("REQUIRED", "(MUST HAVE)");
    app.formatter(optfmt);

    int v{0};
    app.add_option("--opt", v, "Something")->required();

    std::string help = app.help();

    CHECK_THAT(help, Contains("(MUST HAVE)"));
    CHECK_THAT(help, Contains("Something"));
    CHECK_THAT(help, Contains("--opt INT"));
    CHECK_THAT(help, Contains("-h,   --help           Print"));
}

TEST_CASE("Formatter: OptCustomizeSimple", "[formatter]") {
    CLI::App app{"My prog"};

    app.get_formatter()->column_width(25);
    app.get_formatter()->label("REQUIRED", "(MUST HAVE)");

    int v{0};
    app.add_option("--opt", v, "Something")->required();

    std::string help = app.help();

    CHECK_THAT(help, Contains("(MUST HAVE)"));
    CHECK_THAT(help, Contains("(MUST HAVE)"));
    CHECK_THAT(help, Contains("Something"));
    CHECK_THAT(help, Contains("--opt INT"));
    CHECK_THAT(help, Contains("-h,   --help           Print"));
}

TEST_CASE("Formatter: OptCustomizeOptionText", "[formatter]") {
    CLI::App app{"My prog"};

    app.get_formatter()->column_width(25);

    int v{0};
    app.add_option("--opt", v, "Something")->option_text("(ARG)");

    std::string help = app.help();

    CHECK_THAT(help, Contains("(ARG)"));
}

TEST_CASE("Formatter: FalseFlagExample", "[formatter]") {
    CLI::App app{"My prog"};

    app.get_formatter()->column_width(25);
    app.get_formatter()->label("REQUIRED", "(MUST HAVE)");

    int v{0};
    app.add_flag("--opt,!--no_opt", v, "Something");

    bool flag{false};
    app.add_flag("!-O,--opt2,--no_opt2{false}", flag, "Something else");

    std::string help = app.help();

    CHECK_THAT(help, Contains("--no_opt{false}"));
    CHECK_THAT(help, Contains("--no_opt2{false}"));
    CHECK_THAT(help, Contains("-O{false}"));
}

TEST_CASE("Formatter: AppCustomize", "[formatter]") {
    CLI::App app{"My prog"};
    app.add_subcommand("subcom1", "This");

    auto appfmt = std::make_shared<CLI::Formatter>();
    appfmt->column_width(20);
    appfmt->label("Usage", "Run");
    app.formatter(appfmt);

    app.add_subcommand("subcom2", "That");

    std::string help = app.help();
    CHECK_THAT(help, Contains("Run: [OPTIONS] [SUBCOMMAND]\n\n"));
    CHECK_THAT(help, Contains("\nSUBCOMMANDS:\n"));
    CHECK_THAT(help, Contains("  subcom1           This \n"));
    CHECK_THAT(help, Contains("  subcom2           That \n"));
}

TEST_CASE("Formatter: AppCustomizeSimple", "[formatter]") {
    CLI::App app{"My prog"};
    app.add_subcommand("subcom1", "This");

    app.get_formatter()->column_width(20);
    app.get_formatter()->label("Usage", "Run");

    app.add_subcommand("subcom2", "That");

    std::string help = app.help();
    CHECK_THAT(help, Contains("Run: [OPTIONS] [SUBCOMMAND]\n\n"));
    CHECK_THAT(help, Contains("\nSUBCOMMANDS:\n"));
    CHECK_THAT(help, Contains("  subcom1           This \n"));
    CHECK_THAT(help, Contains("  subcom2           That \n"));
}

TEST_CASE("Formatter: AllSub", "[formatter]") {
    CLI::App app{"My prog"};
    CLI::App *sub = app.add_subcommand("subcom", "This");
    sub->add_flag("--insub", "MyFlag");

    std::string help = app.help("", CLI::AppFormatMode::All);
    CHECK_THAT(help, Contains("--insub"));
    CHECK_THAT(help, Contains("subcom"));
}

TEST_CASE("Formatter: AllSubRequired", "[formatter]") {
    CLI::App app{"My prog"};
    CLI::App *sub = app.add_subcommand("subcom", "This");
    sub->add_flag("--insub", "MyFlag");
    sub->required();
    std::string help = app.help("", CLI::AppFormatMode::All);
    CHECK_THAT(help, Contains("--insub"));
    CHECK_THAT(help, Contains("subcom"));
    CHECK_THAT(help, Contains("REQUIRED"));
}

TEST_CASE("Formatter: NamelessSub", "[formatter]") {
    CLI::App app{"My prog"};
    CLI::App *sub = app.add_subcommand("", "This subcommand");
    sub->add_flag("--insub", "MyFlag");

    std::string help = app.help("", CLI::AppFormatMode::Normal);
    CHECK_THAT(help, Contains("--insub"));
    CHECK_THAT(help, Contains("This subcommand"));
}

TEST_CASE("Formatter: NamelessSubInGroup", "[formatter]") {
    CLI::App app{"My prog"};
    CLI::App *sub = app.add_subcommand("", "This subcommand");
    CLI::App *sub2 = app.add_subcommand("sub2", "subcommand2");
    sub->add_flag("--insub", "MyFlag");
    int val{0};
    sub2->add_option("pos", val, "positional");
    sub->group("group1");
    sub2->group("group1");
    std::string help = app.help("", CLI::AppFormatMode::Normal);
    CHECK_THAT(help, Contains("--insub"));
    CHECK_THAT(help, Contains("This subcommand"));
    CHECK_THAT(help, Contains("group1"));
    CHECK_THAT(help, Contains("sub2"));
    CHECK(help.find("pos") == std::string::npos);
}
