// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <cstdio>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

TEST_CASE("StringBased: convert_arg_for_ini", "[config]") {

    CHECK("\"\"" == CLI::detail::convert_arg_for_ini(std::string{}));

    CHECK("true" == CLI::detail::convert_arg_for_ini("true"));

    CHECK("nan" == CLI::detail::convert_arg_for_ini("nan"));

    CHECK("\"happy hippo\"" == CLI::detail::convert_arg_for_ini("happy hippo"));

    CHECK("47" == CLI::detail::convert_arg_for_ini("47"));

    CHECK("47.365225" == CLI::detail::convert_arg_for_ini("47.365225"));

    CHECK("+3.28e-25" == CLI::detail::convert_arg_for_ini("+3.28e-25"));
    CHECK("-22E14" == CLI::detail::convert_arg_for_ini("-22E14"));

    CHECK("'a'" == CLI::detail::convert_arg_for_ini("a"));

    CHECK("'\\'" == CLI::detail::convert_arg_for_ini("\\"));

    CHECK("\"'\"" == CLI::detail::convert_arg_for_ini("'"));

    std::string tstring1;
    tstring1.push_back('\0');
    // binary string conversion single character
    CHECK("'B\"(\\x00)\"'" == CLI::detail::convert_arg_for_ini(tstring1));
    // hex
    CHECK("0x5461FAED" == CLI::detail::convert_arg_for_ini("0x5461FAED"));
    // hex fail
    CHECK("\"0x5461FAEG\"" == CLI::detail::convert_arg_for_ini("0x5461FAEG"));

    // octal
    CHECK("0o546123567" == CLI::detail::convert_arg_for_ini("0o546123567"));
    // octal fail
    CHECK("\"0o546123587\"" == CLI::detail::convert_arg_for_ini("0o546123587"));

    // binary
    CHECK("0b01101110010" == CLI::detail::convert_arg_for_ini("0b01101110010"));
    // binary fail
    CHECK("\"0b01102110010\"" == CLI::detail::convert_arg_for_ini("0b01102110010"));
}

TEST_CASE("StringBased: IniJoin", "[config]") {
    std::vector<std::string> items = {"one", "two", "three four"};
    std::string result = R"("one" "two" "three four")";

    CHECK(result == CLI::detail::ini_join(items, ' ', '\0', '\0'));

    result = R"(["one", "two", "three four"])";

    CHECK(result == CLI::detail::ini_join(items));

    result = R"({"one"; "two"; "three four"})";

    CHECK(result == CLI::detail::ini_join(items, ';', '{', '}'));
}

TEST_CASE("StringBased: First", "[config]") {
    std::stringstream ofile;

    ofile << "one=three\n";
    ofile << "two=four\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 2u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "four");
}

TEST_CASE("StringBased: FirstWithComments", "[config]") {
    std::stringstream ofile;

    ofile << ";this is a comment\n";
    ofile << "one=three\n";
    ofile << "two=four\n";
    ofile << "; and another one\n";
    ofile << "   ; and yet another one\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 2u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "four");
}

TEST_CASE("StringBased: Quotes", "[config]") {
    std::stringstream ofile;

    ofile << R"(one = "three")" << '\n';
    ofile << R"(two = 'four')" << '\n';
    ofile << R"(five = "six and seven")" << '\n';

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "four");
    CHECK(output.at(2).name == "five");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "six and seven");
}

TEST_CASE("StringBased: Vector", "[config]") {
    std::stringstream ofile;

    ofile << "one = three\n";
    ofile << "two = four\n";
    ofile << "five = six and seven\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "four");
    CHECK(output.at(2).name == "five");
    CHECK(output.at(2).inputs.size() == 3u);
    CHECK(output.at(2).inputs.at(0) == "six");
    CHECK(output.at(2).inputs.at(1) == "and");
    CHECK(output.at(2).inputs.at(2) == "seven");
}

TEST_CASE("StringBased: TomlVector", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = [four]\n";
    ofile << "five = [six, and, seven]\n";
    ofile << "eight = [nine, \n"
             "ten, eleven,     twelve    \n"
             "]\n";
    ofile << "one_more = [one, \n"
             "two,     three  ]    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 5u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "four");
    CHECK(output.at(2).name == "five");
    CHECK(output.at(2).inputs.size() == 3u);
    CHECK(output.at(2).inputs.at(0) == "six");
    CHECK(output.at(2).inputs.at(1) == "and");
    CHECK(output.at(2).inputs.at(2) == "seven");
    CHECK(output.at(3).name == "eight");
    CHECK(output.at(3).inputs.size() == 4u);
    CHECK(output.at(3).inputs.at(0) == "nine");
    CHECK(output.at(3).inputs.at(1) == "ten");
    CHECK(output.at(3).inputs.at(2) == "eleven");
    CHECK(output.at(3).inputs.at(3) == "twelve");
    CHECK(output.at(4).name == "one_more");
    CHECK(output.at(4).inputs.size() == 3u);
    CHECK(output.at(4).inputs.at(0) == "one");
    CHECK(output.at(4).inputs.at(1) == "two");
    CHECK(output.at(4).inputs.at(2) == "three");
}

TEST_CASE("StringBased: TomlMultiLineString1", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = \"\"\"test\n";
    ofile << "five = [six, and, seven]\n";
    ofile << "eight\"\"\"\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "test\nfive = [six, and, seven]\neight");
    CHECK(output.at(2).name == "three");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "7");
}

TEST_CASE("StringBased: TomlMultiLineString2", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = '''test  \n";
    ofile << "five = [six, and, seven] \n";
    ofile << "'''\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "test  \nfive = [six, and, seven] ");
    CHECK(output.at(2).name == "three");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "7");
}

TEST_CASE("StringBased: TomlMultiLineString3", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = \"\"\"\n";
    ofile << "test \\\n";
    ofile << "     five = [six, and, seven] \\\n";
    ofile << "eight\"\"\"\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "test five = [six, and, seven] eight");
    CHECK(output.at(2).name == "three");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "7");
}

TEST_CASE("StringBased: TomlMultiLineString4", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = \"\"\"\n";
    ofile << "test\n";
    ofile << "five = [six, and, seven]\n";
    ofile << "\"\"\"\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "test\nfive = [six, and, seven]");
    CHECK(output.at(2).name == "three");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "7");
}

TEST_CASE("StringBased: TomlMultiLineString5", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = \"\"\" mline \\\n";
    ofile << "test\n";
    ofile << '\n';
    ofile << "five = [six, and, seven]\n";
    ofile << "\"\"\"\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == " mline test\n\nfive = [six, and, seven]");
    CHECK(output.at(2).name == "three");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "7");
}

TEST_CASE("StringBased: TomlMultiLineString6", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = \"\"\" mline this is a long line \"\"\"\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 3u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == " mline this is a long line ");
    CHECK(output.at(2).name == "three");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "7");
}

TEST_CASE("StringBased: TomlMultiLineStringError", "[config]") {
    std::stringstream ofile;

    ofile << "one = [three]\n";
    ofile << "two = \"\"\" mline this\\7 is a long line \"\"\"\n";
    ofile << "three=7    \n";

    ofile.seekg(0, std::ios::beg);

    CHECK_THROWS(CLI::ConfigINI().from_config(ofile));
}

TEST_CASE("StringBased: Spaces", "[config]") {
    std::stringstream ofile;

    ofile << "one = three\n";
    ofile << "two = four";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 2u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).name == "two");
    CHECK(output.at(1).inputs.size() == 1u);
    CHECK(output.at(1).inputs.at(0) == "four");
}

TEST_CASE("StringBased: Sections", "[config]") {
    std::stringstream ofile;

    ofile << "one=three\n";
    ofile << "[second]\n";
    ofile << "  two=four\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 4u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(2).name == "two");
    CHECK(output.at(2).parents.at(0) == "second");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "four");
    CHECK(output.at(2).fullname() == "second.two");
}

TEST_CASE("StringBased: SpacesSections", "[config]") {
    std::stringstream ofile;

    ofile << "one=three\n\n";
    ofile << "[second]   \n";
    ofile << "   \n";
    ofile << "  two=four\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    CHECK(output.size() == 4u);
    CHECK(output.at(0).name == "one");
    CHECK(output.at(0).inputs.size() == 1u);
    CHECK(output.at(0).inputs.at(0) == "three");
    CHECK(output.at(1).parents.at(0) == "second");
    CHECK(output.at(1).name == "++");
    CHECK(output.at(2).name == "two");
    CHECK(output.at(2).parents.size() == 1u);
    CHECK(output.at(2).parents.at(0) == "second");
    CHECK(output.at(2).inputs.size() == 1u);
    CHECK(output.at(2).inputs.at(0) == "four");
    CHECK(output.at(3).parents.at(0) == "second");
    CHECK(output.at(3).name == "--");
}

// check function to make sure that open sections match close sections
bool checkSections(const std::vector<CLI::ConfigItem> &output) {
    std::set<std::string> open;
    for(const auto &ci : output) {
        if(ci.name == "++") {
            auto nm = ci.fullname();
            nm.pop_back();
            nm.pop_back();
            auto rv = open.insert(nm);
            if(!rv.second) {
                return false;
            }
        }
        if(ci.name == "--") {
            auto nm = ci.fullname();
            nm.pop_back();
            nm.pop_back();
            auto rv = open.erase(nm);
            if(rv != 1U) {
                return false;
            }
        }
    }
    return open.empty();
}
TEST_CASE("StringBased: Layers", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other]\n";
    ofile << "[other.sub2]\n";
    ofile << "[other.sub2.sub-level2]\n";
    ofile << "[other.sub2.sub-level2.sub-level3]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 4 openings and 4 closings
    CHECK(output.size() == 10u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: LayersSkip", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other.sub2]\n";
    ofile << "[other.sub2.sub-level2.sub-level3]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 4 openings and 4 closings
    CHECK(output.size() == 10u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: LayersSkipOrdered", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other.sub2.sub-level2.sub-level3]\n";
    ofile << "[other.sub2]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 4 openings and 4 closings
    CHECK(output.size() == 12u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: LayersChange", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other.sub2]\n";
    ofile << "[other.sub3]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 3 openings and 3 closings
    CHECK(output.size() == 8u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: Layers2LevelChange", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other.sub2.cmd]\n";
    ofile << "[other.sub3.cmd]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 5 openings and 5 closings
    CHECK(output.size() == 12u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: Layers2LevelChangeInQuotes", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[\"other\".\"sub2\".cmd]\n";
    ofile << "[other.\"sub3\".\"cmd\"]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 5 openings and 5 closings
    CHECK(output.size() == 12u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: Layers2LevelChangeInQuotesWithDot", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[\"other\".\"sub2.cmd\"]\n";
    ofile << "[other.\"sub3.cmd\"]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 3 openings and 3 closings
    CHECK(output.size() == 8u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: Layers3LevelChange", "[config]") {
    std::stringstream ofile;

    ofile << "[other.sub2.subsub.cmd]\n";
    ofile << "[other.sub3.subsub.cmd]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 1 flags and 7 openings and 7 closings
    CHECK(output.size() == 15u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: newSegment", "[config]") {
    std::stringstream ofile;

    ofile << "[other.sub2.subsub.cmd]\n";
    ofile << "flag = true\n";
    ofile << "[another]\n";
    ofile << "absolute_newest = true\n";
    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 5 openings and 5 closings
    CHECK(output.size() == 12u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: LayersDirect", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other.sub2.sub-level2.sub-level3]\n";
    ofile << "absolute_newest = true\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 2 flags and 4 openings and 4 closings
    CHECK(output.size() == 10u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: LayersComplex", "[config]") {
    std::stringstream ofile;

    ofile << "simple = true\n\n";
    ofile << "[other.sub2.sub-level2.sub-level3]\n";
    ofile << "absolute_newest = true\n";
    ofile << "[other.sub2.sub-level2]\n";
    ofile << "still_newer = true\n";
    ofile << "[other.sub2]\n";
    ofile << "newest = true\n";

    ofile.seekg(0, std::ios::beg);

    std::vector<CLI::ConfigItem> output = CLI::ConfigINI().from_config(ofile);

    // 4 flags and 6 openings and 6 closings
    CHECK(output.size() == 16u);
    CHECK(checkSections(output));
}

TEST_CASE("StringBased: file_error", "[config]") {
    CHECK_THROWS_AS(CLI::ConfigINI().from_file("nonexist_file"), CLI::FileError);
}

static const int fclear1 = fileClear("TestIniTmp.ini");

TEST_CASE_METHOD(TApp, "IniNotRequired", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    int one = 0, two = 0, three = 0;
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    args = {"--one=1"};

    run();

    CHECK(one == 1);
    CHECK(two == 99);
    CHECK(three == 3);

    one = two = three = 0;
    args = {"--one=1", "--two=2"};

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);
    CHECK("TestIniTmp.ini" == app["--config"]->as<std::string>());
}

TEST_CASE_METHOD(TApp, "IniSuccessOnUnknownOption", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.allow_config_extras(true);

    {
        std::ofstream out{tmpini};
        out << "three=3" << '\n';
        out << "two=99" << '\n';
    }

    int two{0};
    app.add_option("--two", two);
    run();
    CHECK(two == 99);
}

TEST_CASE_METHOD(TApp, "IniGetRemainingOption", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.allow_config_extras(true);

    std::string ExtraOption = "three";
    std::string ExtraOptionValue = "3";
    {
        std::ofstream out{tmpini};
        out << ExtraOption << "=" << ExtraOptionValue << '\n';
        out << "two=99" << '\n';
    }

    int two{0};
    app.add_option("--two", two);
    REQUIRE_NOTHROW(run());
    std::vector<std::string> ExpectedRemaining = {ExtraOption, ExtraOptionValue};
    CHECK(ExpectedRemaining == app.remaining());
}

TEST_CASE_METHOD(TApp, "IniIgnoreRemainingOption", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.allow_config_extras(CLI::config_extras_mode::ignore);

    {
        std::ofstream out{tmpini};
        out << "three=3\n";
        out << "two=99\n";
    }

    int two{0};
    app.add_option("--two", two);
    REQUIRE_NOTHROW(run());
    std::vector<std::string> ExpectedRemaining = {};
    CHECK(ExpectedRemaining == app.remaining());
}

TEST_CASE_METHOD(TApp, "IniRemainingSub", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    auto *map = app.add_subcommand("map");
    map->allow_config_extras();

    {
        std::ofstream out{tmpini};
        out << "[map]\n";
        out << "a = 1\n";
        out << "b=[1,2,3]\n";
        out << "c = 3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    std::vector<std::string> rem = map->remaining();
    REQUIRE(rem.size() == 8U);
    CHECK(rem[0] == "map.a");
    CHECK(rem[2] == "map.b");
    CHECK(rem[6] == "map.c");
    CHECK(rem[5] == "3");

    int a{0};
    int c{0};
    std::vector<int> b;
    map->add_option("-a", a);
    map->add_option("-b", b);
    map->add_option("-c", c);

    CHECK_NOTHROW(app.parse(app.remaining_for_passthrough()));
    CHECK(a == 1);
    CHECK(c == 3);
    REQUIRE(b.size() == 3U);
    CHECK(b[1] == 2);
}

TEST_CASE_METHOD(TApp, "IniGetNoRemaining", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.allow_config_extras(true);

    {
        std::ofstream out{tmpini};
        out << "two=99" << '\n';
    }

    int two{0};
    app.add_option("--two", two);
    REQUIRE_NOTHROW(run());
    CHECK(app.remaining().empty());
}

TEST_CASE_METHOD(TApp, "IniRequiredNoDefault", "[config]") {

    app.set_config("--config")->required();

    int two{0};
    app.add_option("--two", two);
    REQUIRE_THROWS_AS(run(), CLI::FileError);
    // test to make sure help still gets called correctly
    // GitHub issue #533 https://github.com/CLIUtils/CLI11/issues/553
    args = {"--help"};
    REQUIRE_THROWS_AS(run(), CLI::CallForHelp);
}

TEST_CASE_METHOD(TApp, "IniNotRequiredNoDefault", "[config]") {

    app.set_config("--config");

    int two{0};
    app.add_option("--two", two);
    REQUIRE_NOTHROW(run());
}

/// Define a class for testing purposes that does bad things
class EvilConfig : public CLI::Config {
  public:
    EvilConfig() = default;
    std::string to_config(const CLI::App *, bool, bool, std::string) const override { throw CLI::FileError("evil"); }

    std::vector<CLI::ConfigItem> from_config(std::istream &) const override { throw CLI::FileError("evil"); }
};

TEST_CASE_METHOD(TApp, "IniRequiredbadConfigurator", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    app.set_config("--config", tmpini)->required();
    app.config_formatter(std::make_shared<EvilConfig>());
    int two{0};
    app.add_option("--two", two);
    REQUIRE_THROWS_AS(run(), CLI::FileError);
}

TEST_CASE_METHOD(TApp, "IniNotRequiredbadConfigurator", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    app.set_config("--config", tmpini);
    app.config_formatter(std::make_shared<EvilConfig>());
    int two{0};
    app.add_option("--two", two);
    REQUIRE_NOTHROW(run());
}

static const int fclear2 = fileClear("TestIniTmp2.ini");

TEST_CASE_METHOD(TApp, "IniNotRequiredNotDefault", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    TempFile tmpini2{"TestIniTmp2.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    {
        std::ofstream out{tmpini2};
        out << "[default]" << '\n';
        out << "two=98" << '\n';
        out << "three=4" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    run();
    CHECK(tmpini.c_str() == app["--config"]->as<std::string>());
    CHECK(two == 99);
    CHECK(three == 3);

    args = {"--config", tmpini2};
    run();

    CHECK(two == 98);
    CHECK(three == 4);
    CHECK(tmpini2.c_str() == app.get_config_ptr()->as<std::string>());
}

TEST_CASE_METHOD(TApp, "IniEnvironmentalFileName", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", "")->envname("CONFIG")->required();

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    put_env("CONFIG", tmpini);

    CHECK_NOTHROW(run());

    CHECK(two == 99);
    CHECK(three == 3);

    unset_env("CONFIG");

    CHECK_THROWS_AS(run(), CLI::FileError);
}

TEST_CASE_METHOD(TApp, "MultiConfig", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    TempFile tmpini2{"TestIniTmp2.ini"};

    app.set_config("--config")->expected(1, 3);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    {
        std::ofstream out{tmpini2};
        out << "[default]" << '\n';
        out << "one=55" << '\n';
        out << "three=4" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    args = {"--config", tmpini2, "--config", tmpini};
    run();

    CHECK(two == 99);
    CHECK(three == 3);
    CHECK(one == 55);

    args = {"--config", tmpini, "--config", tmpini2};
    run();

    CHECK(two == 99);
    CHECK(three == 4);
    CHECK(one == 55);
}

TEST_CASE_METHOD(TApp, "MultiConfig_takelast", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    TempFile tmpini2{"TestIniTmp2.ini"};

    app.set_config("--config")->multi_option_policy(CLI::MultiOptionPolicy::TakeLast)->expected(1, 3);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    {
        std::ofstream out{tmpini2};
        out << "[default]" << '\n';
        out << "one=55" << '\n';
        out << "three=4" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    args = {"--config", tmpini, "--config", tmpini2};
    run();

    CHECK(two == 99);
    CHECK(three == 3);
    CHECK(one == 55);

    two = 0;
    args = {"--config", tmpini2, "--config", tmpini};
    run();

    CHECK(two == 99);
    CHECK(three == 4);
    CHECK(one == 55);
}

TEST_CASE_METHOD(TApp, "MultiConfig_takeAll", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    TempFile tmpini2{"TestIniTmp2.ini"};

    app.set_config("--config")->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    {
        std::ofstream out{tmpini2};
        out << "[default]" << '\n';
        out << "one=55" << '\n';
        out << "three=4" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    args = {"--config", tmpini, "--config", tmpini2};
    run();

    CHECK(two == 99);
    CHECK(three == 3);
    CHECK(one == 55);

    two = 0;
    args = {"--config", tmpini2, "--config", tmpini};
    run();

    CHECK(two == 99);
    CHECK(three == 4);
    CHECK(one == 55);
}

TEST_CASE_METHOD(TApp, "MultiConfig_single", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    TempFile tmpini2{"TestIniTmp2.ini"};

    app.set_config("--config")->multi_option_policy(CLI::MultiOptionPolicy::TakeLast);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    {
        std::ofstream out{tmpini2};
        out << "[default]" << '\n';
        out << "one=55" << '\n';
        out << "three=4" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    args = {"--config", tmpini2, "--config", tmpini};
    run();

    CHECK(two == 99);
    CHECK(three == 3);
    CHECK(one == 0);

    two = 0;
    args = {"--config", tmpini, "--config", tmpini2};
    run();

    CHECK(two == 0);
    CHECK(three == 4);
    CHECK(one == 55);
}

TEST_CASE_METHOD(TApp, "IniRequiredNotFound", "[config]") {

    std::string noini = "TestIniNotExist.ini";
    app.set_config("--config", noini, "", true);

    CHECK_THROWS_AS(run(), CLI::FileError);
}

TEST_CASE_METHOD(TApp, "IniDefaultNotExist", "[config]") {

    std::string noini = "TestIniNotExist.ini";
    auto *cfg = app.set_config("--config", noini);

    CHECK_NOTHROW(run());

    CHECK(cfg->count() == 0);
}

TEST_CASE_METHOD(TApp, "IniNotRequiredPassedNotFound", "[config]") {

    std::string noini = "TestIniNotExist.ini";
    app.set_config("--config", "", "", false);

    args = {"--config", noini};
    CHECK_THROWS_AS(run(), CLI::FileError);
}

TEST_CASE_METHOD(TApp, "IniOverwrite", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
    }

    std::string orig = "filename_not_exist.ini";
    std::string next = "TestIniTmp.ini";
    app.set_config("--config", orig);
    // Make sure this can be overwritten
    app.set_config("--conf", next);
    int two{7};
    app.add_option("--two", two);

    run();

    CHECK(two == 99);
}

TEST_CASE_METHOD(TApp, "hInConfig", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "h=99" << '\n';
    }

    std::string next = "TestIniTmp.ini";
    app.set_config("--conf", next);
    int two{7};
    app.add_option("--h", two);

    run();

    CHECK(two == 99);
}

TEST_CASE_METHOD(TApp, "notConfigurableOptionOverload", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "m=99" << '\n';
    }

    std::string next = "TestIniTmp.ini";
    app.set_config("--conf", next);
    int two{7};
    int three{5};
    app.add_option("--m", three)->configurable(false);
    app.add_option("-m", two);

    run();
    CHECK(three == 5);
    CHECK(two == 99);
}

TEST_CASE_METHOD(TApp, "notConfigurableOptionOverload2", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "m=99" << '\n';
    }

    std::string next = "TestIniTmp.ini";
    app.set_config("--conf", next);
    int two{7};
    int three{5};
    app.add_option("-m", three)->configurable(false);
    app.add_option("m", two);

    run();
    CHECK(three == 5);
    CHECK(two == 99);
}

TEST_CASE_METHOD(TApp, "IniRequired", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    auto *cfg = app.set_config("--config", tmpini, "", true);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one)->required();
    app.add_option("--two", two)->required();
    app.add_option("--three", three)->required();

    args = {"--one=1"};

    run();
    CHECK(1 == one);
    CHECK(99 == two);
    CHECK(3 == three);

    one = two = three = 0;
    args = {"--one=1", "--two=2"};

    CHECK_NOTHROW(run());
    CHECK(cfg->count() == 1);
    CHECK(1 == one);
    CHECK(2 == two);
    CHECK(3 == three);

    args = {};

    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--two=2"};

    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(TApp, "IniInlineComment", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini, "", true);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99 ; this is a two" << '\n';
        out << "three=3; this is a three" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one)->required();
    app.add_option("--two", two)->required();
    app.add_option("--three", three)->required();

    args = {"--one=1"};

    run();
    CHECK(1 == one);
    CHECK(99 == two);
    CHECK(3 == three);

    one = two = three = 0;
    args = {"--one=1", "--two=2"};

    CHECK_NOTHROW(run());
    CHECK(1 == one);
    CHECK(2 == two);
    CHECK(3 == three);

    args = {};

    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--two=2"};

    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(TApp, "TomlInlineComment", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini, "", true);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99 # this is a two" << '\n';
        out << "three=3# this is a three" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one)->required();
    app.add_option("--two", two)->required();
    app.add_option("--three", three)->required();

    args = {"--one=1"};

    run();
    CHECK(1 == one);
    CHECK(99 == two);
    CHECK(3 == three);

    one = two = three = 0;
    args = {"--one=1", "--two=2"};

    CHECK_NOTHROW(run());
    CHECK(1 == one);
    CHECK(2 == two);
    CHECK(3 == three);

    args = {};

    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"--two=2"};

    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(TApp, "TomlDocStringComment", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini, "", true);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
        out << R"(""")" << '\n';
        out << "one=35" << '\n';
        out << R"(""")" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    CHECK_NOTHROW(run());
    CHECK(0 == one);
    CHECK(99 == two);
    CHECK(3 == three);
}

TEST_CASE_METHOD(TApp, "TomlDocStringComment2", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini, "", true);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "'''" << '\n';
        out << "one=35" << '\n';
        out << "last comment line three=6 '''" << '\n';
        out << "three=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    CHECK_NOTHROW(run());
    CHECK(0 == one);
    CHECK(99 == two);
    CHECK(3 == three);
}

TEST_CASE_METHOD(TApp, "TomlDocStringComment3", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini, "", true);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=99" << '\n';
        out << "three=3" << '\n';
        out << "'''" << '\n';
        out << "one=35" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--one", one);
    app.add_option("--two", two);
    app.add_option("--three", three);

    CHECK_NOTHROW(run());
    CHECK(0 == one);
    CHECK(99 == two);
    CHECK(3 == three);
}

TEST_CASE_METHOD(TApp, "ConfigModifiers", "[config]") {

    app.set_config("--config", "test.ini", "", true);

    auto cfgptr = app.get_config_formatter_base();

    cfgptr->section("test");
    CHECK(cfgptr->section() == "test");

    CHECK(cfgptr->sectionRef() == "test");
    auto &sref = cfgptr->sectionRef();
    sref = "this";
    CHECK(cfgptr->section() == "this");

    cfgptr->index(5);
    CHECK(cfgptr->index() == 5);

    CHECK(cfgptr->indexRef() == 5);
    auto &iref = cfgptr->indexRef();
    iref = 7;
    CHECK(cfgptr->index() == 7);
}

TEST_CASE_METHOD(TApp, "IniVector", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2 3" << '\n';
        out << "three=1 2 3" << '\n';
    }

    std::vector<int> two, three;
    app.add_option("--two", two)->expected(2)->required();
    app.add_option("--three", three)->required();

    run();

    CHECK(two == std::vector<int>({2, 3}));
    CHECK(three == std::vector<int>({1, 2, 3}));
}

TEST_CASE_METHOD(TApp, "IniFlagOverride", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "three=0" << '\n';
    }

    int flag{45};
    app.add_flag("--two{2},--three{3},--four{4}", flag)->disable_flag_override()->force_callback()->default_str("0");

    run();

    CHECK(flag == 0);
}

TEST_CASE_METHOD(TApp, "TOMLVector", "[config]") {

    TempFile tmptoml{"TestTomlTmp.toml"};

    app.set_config("--config", tmptoml);

    {
        std::ofstream out{tmptoml};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two=[2,3]\n";
        out << "three=[1,2,3]\n";
    }

    std::vector<int> two, three;
    app.add_option("--two", two)->expected(2)->required();
    app.add_option("--three", three)->required();

    run();

    CHECK(two == std::vector<int>({2, 3}));
    CHECK(three == std::vector<int>({1, 2, 3}));
}

TEST_CASE_METHOD(TApp, "ColonValueSep", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two:2\n";
        out << "three:3\n";
    }

    int two{0}, three{0};
    app.add_option("--two", two);
    app.add_option("--three", three);

    app.get_config_formatter_base()->valueSeparator(':');

    run();

    CHECK(two == 2);
    CHECK(three == 3);
}

TEST_CASE_METHOD(TApp, "TOMLVectordirect", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    app.config_formatter(std::make_shared<CLI::ConfigTOML>());

    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two=[2,3]\n";
        out << "three=[1,2,3]\n";
    }

    std::vector<int> two, three;
    app.add_option("--two", two)->expected(2)->required();
    app.add_option("--three", three)->required();

    run();

    CHECK(two == std::vector<int>({2, 3}));
    CHECK(three == std::vector<int>({1, 2, 3}));
}

TEST_CASE_METHOD(TApp, "TOMLVectorVector", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    app.config_formatter(std::make_shared<CLI::ConfigTOML>());

    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two=1,2,3\n";
        out << "two= 4, 5, 6\n";
        out << "three=1,2,3\n";
        out << "three= 4, 5, 6\n";
        out << "four=1,2\n";
        out << "four= 3,4\n";
        out << "four=5,6\n";
        out << "four= 7,8\n";
    }

    std::vector<std::vector<int>> two;
    std::vector<int> three, four;
    app.add_option("--two", two)->delimiter(',');
    app.add_option("--three", three)->delimiter(',');
    app.add_option("--four", four)->delimiter(',');

    run();

    auto str = app.config_to_str();
    CHECK(two == std::vector<std::vector<int>>({{1, 2, 3}, {4, 5, 6}}));
    CHECK(three == std::vector<int>({1, 2, 3, 4, 5, 6}));
    CHECK(four == std::vector<int>({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST_CASE_METHOD(TApp, "TOMLVectorVectorSeparated", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    app.config_formatter(std::make_shared<CLI::ConfigTOML>());
    app.get_config_formatter_base()->allowDuplicateFields();
    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two=1,2,3\n";
        out << "three=1,2,3\n";
        out << "three= 4, 5, 6\n";
        out << "two= 4, 5, 6\n";
    }

    std::vector<std::vector<int>> two;
    std::vector<int> three;
    app.add_option("--two", two)->delimiter(',');
    app.add_option("--three", three)->delimiter(',');

    run();

    auto str = app.config_to_str();
    CHECK(two == std::vector<std::vector<int>>({{1, 2, 3}, {4, 5, 6}}));
    CHECK(three == std::vector<int>({1, 2, 3, 4, 5, 6}));
}

TEST_CASE_METHOD(TApp, "TOMLVectorVectorSeparatedSingleElement", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    app.config_formatter(std::make_shared<CLI::ConfigTOML>());
    app.get_config_formatter_base()->allowDuplicateFields();
    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two=1\n";
        out << "three=1\n";
        out << "three= 4\n";
        out << "three= 5\n";
        out << "two= 2\n";
        out << "two=3\n";
    }

    std::vector<std::vector<int>> two;
    std::vector<int> three;
    app.add_option("--two", two)->delimiter(',');
    app.add_option("--three", three)->delimiter(',');

    run();

    auto str = app.config_to_str();
    CHECK(two == std::vector<std::vector<int>>({{1}, {2}, {3}}));
    CHECK(three == std::vector<int>({1, 4, 5}));
}

TEST_CASE_METHOD(TApp, "TOMLStringVector", "[config]") {

    TempFile tmptoml{"TestTomlTmp.toml"};

    app.set_config("--config", tmptoml);

    {
        std::ofstream out{tmptoml};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "zero1=[]\n";
        out << "zero2={}\n";
        out << "zero3={}\n";
        out << "zero4=[\"{}\",\"\"]\n";
        out << "nzero={}\n";
        out << "one=[\"1\"]\n";
        out << "two=[\"2\",\"3\"]\n";
        out << "three=[\"1\",\"2\",\"3\"]\n";
    }

    std::vector<std::string> nzero, zero1, zero2, zero3, zero4, one, two, three;
    app.add_option("--zero1", zero1)->required()->expected(0, 99)->default_str("{}");
    app.add_option("--zero2", zero2)->required()->expected(0, 99)->default_val(std::vector<std::string>{});
    // if no default is specified the argument results in an empty string
    app.add_option("--zero3", zero3)->required()->expected(0, 99);
    app.add_option("--zero4", zero4)->required()->expected(0, 99);
    app.add_option("--nzero", nzero)->required();
    app.add_option("--one", one)->required();
    app.add_option("--two", two)->required();
    app.add_option("--three", three)->required();

    run();

    CHECK(zero1 == std::vector<std::string>({}));
    CHECK(zero2 == std::vector<std::string>({}));
    CHECK(zero3 == std::vector<std::string>({}));
    CHECK(zero4 == std::vector<std::string>({"{}"}));
    CHECK(nzero == std::vector<std::string>({"{}"}));
    CHECK(one == std::vector<std::string>({"1"}));
    CHECK(two == std::vector<std::string>({"2", "3"}));
    CHECK(three == std::vector<std::string>({"1", "2", "3"}));
}

TEST_CASE_METHOD(TApp, "IniVectorCsep", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "zero1=[]\n";
        out << "zero2=[]\n";
        out << "one=[1]\n";
        out << "two=[2,3]\n";
        out << "three=1,2,3\n";
    }

    std::vector<int> zero1, zero2, one, two, three;
    app.add_option("--zero1", zero1)->required()->expected(0, 99)->default_str("{}");
    app.add_option("--zero2", zero2)->required()->expected(0, 99)->default_val(std::vector<int>{});
    app.add_option("--one", one)->required();
    app.add_option("--two", two)->expected(2)->required();
    app.add_option("--three", three)->required();

    run();

    CHECK(zero1 == std::vector<int>({}));
    CHECK(zero2 == std::vector<int>({}));
    CHECK(one == std::vector<int>({1}));
    CHECK(two == std::vector<int>({2, 3}));
    CHECK(three == std::vector<int>({1, 2, 3}));
}

TEST_CASE_METHOD(TApp, "IniVectorMultiple", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "#this is a comment line\n";
        out << "[default]\n";
        out << "two=2\n";
        out << "two=3\n";
        out << "three=1\n";
        out << "three=2\n";
        out << "three=3\n";
    }

    std::vector<int> two, three;
    app.add_option("--two", two)->expected(2)->required();
    app.add_option("--three", three)->required();

    run();

    CHECK(two == std::vector<int>({2, 3}));
    CHECK(three == std::vector<int>({1, 2, 3}));
}

TEST_CASE_METHOD(TApp, "IniLayered", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(0U == subcom->count());
    CHECK(!*subcom);
}

TEST_CASE_METHOD(TApp, "IniLayeredStream", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    std::ifstream in{tmpini};
    app.parse_from_stream(in);

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(0U == subcom->count());
    CHECK(!*subcom);
}

TEST_CASE_METHOD(TApp, "IniLayeredDotSection", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "[subcom.subsubcom]" << '\n';
        out << "val=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(0U == subcom->count());
    CHECK(!*subcom);

    three = 0;
    // check maxlayers
    app.get_config_formatter_base()->maxLayers(1);
    run();
    CHECK(three == 0);
}

TEST_CASE_METHOD(TApp, "IniLayeredDotSectionInQuotes", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "['subcom']" << '\n';
        out << "val=2" << '\n';
        out << "['subcom'.\"subsubcom\"]" << '\n';
        out << "val=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(0U == subcom->count());
    CHECK(!*subcom);

    three = 0;
    // check maxlayers
    app.get_config_formatter_base()->maxLayers(1);
    run();
    CHECK(three == 0);
}

TEST_CASE_METHOD(TApp, "IniLayeredCustomSectionSeparator", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "[subcom|subsubcom]" << '\n';
        out << "val=3" << '\n';
    }
    app.get_config_formatter_base()->parentSeparator('|');
    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(0U == subcom->count());
    CHECK(!*subcom);
}

TEST_CASE_METHOD(TApp, "IniLayeredOptionGroupAlias", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[ogroup]" << '\n';
        out << "val2=2" << '\n';
    }
    int one{0}, two{0};
    app.add_option("--val", one);
    auto *subcom = app.add_option_group("ogroup")->alias("ogroup");
    subcom->add_option("--val2", two);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurable", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->configurable();
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(1U == subcom->count());
    CHECK(*subcom);
    CHECK(app.got_subcommand(subcom));
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurableInQuotes", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "\"subsubcom\".'val'=3" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->configurable();
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(1U == subcom->count());
    CHECK(*subcom);
    CHECK(app.got_subcommand(subcom));
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurableInQuotesAlias", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << R"("sub\tsub\t.com".'val'=3)" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->configurable();
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom")->alias("sub\tsub\t.com");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(1U == subcom->count());
    CHECK(*subcom);
    CHECK(app.got_subcommand(subcom));
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurableInQuotesAliasWithEquals", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << R"("sub=sub=.com".'val'=3)" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->configurable();
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom")->alias("sub=sub=.com");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);

    CHECK(1U == subcom->count());
    CHECK(*subcom);
    CHECK(app.got_subcommand(subcom));
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurableHelp", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
    }

    int one{0}, two{0};
    app.add_option("--val", one);
    app.add_option("--helptest", two);
    auto *subcom = app.add_subcommand("subcom");
    subcom->configurable();
    subcom->add_option("--val", two);

    args = {"--help"};
    CHECK_THROWS_AS(run(), CLI::CallForHelp);

    auto helpres = app.help();
    CHECK_THAT(helpres, Contains("--helptest"));
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurableInQuotesAliasWithComment", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << R"("sub#sub;.com".'val'=3)" << '\n';
    }

    int one{0}, two{0}, three{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    subcom->configurable();
    subcom->add_option("--val", two);
    auto *subsubcom = subcom->add_subcommand("subsubcom")->alias("sub#sub;.com");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurablePreParse", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
    }

    int one{0}, two{0}, three{0}, four{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    auto *subcom2 = app.add_subcommand("subcom2");
    subcom->configurable();
    std::vector<std::size_t> parse_c;
    subcom->preparse_callback([&parse_c](std::size_t cnt) { parse_c.push_back(cnt); });
    subcom->add_option("--val", two);
    subcom2->add_option("--val", four);
    subcom2->preparse_callback([&parse_c](std::size_t cnt) { parse_c.push_back(cnt + 2623); });
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);
    CHECK(four == 0);

    CHECK(1U == parse_c.size());
    CHECK(2U == parse_c[0]);

    CHECK(0U == subcom2->count());
}

TEST_CASE_METHOD(TApp, "IniSection", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.get_config_formatter_base()->section("config");

    {
        std::ofstream out{tmpini};
        out << "[config]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
        out << "[default]" << '\n';
        out << "val=1" << '\n';
    }

    int val{0};
    app.add_option("--val", val);

    run();

    CHECK(2 == val);
}

TEST_CASE_METHOD(TApp, "IniSection2", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.get_config_formatter_base()->section("config");

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[config]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
    }

    int val{0};
    app.add_option("--val", val);

    run();

    CHECK(2 == val);
}

TEST_CASE_METHOD(TApp, "jsonLikeParsing", "[config]") {

    TempFile tmpjson{"TestJsonTmp.json"};

    app.set_config("--config", tmpjson);
    app.get_config_formatter_base()->valueSeparator(':');

    {
        std::ofstream out{tmpjson};
        out << "{" << '\n';
        out << "\"val\":1," << '\n';
        out << R"("val2":"test",)" << '\n';
        out << "\"flag\":true" << '\n';
        out << "}" << '\n';
    }

    int val{0};
    app.add_option("--val", val);
    std::string val2{0};
    app.add_option("--val2", val2);

    bool flag{false};
    app.add_flag("--flag", flag);

    run();

    CHECK(1 == val);
    CHECK(val2 == "test");
    CHECK(flag);
}

TEST_CASE_METHOD(TApp, "TomlSectionNumber", "[config]") {

    TempFile tmpini{"TestTomlTmp.toml"};

    app.set_config("--config", tmpini);
    app.get_config_formatter_base()->section("config")->index(0);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[[config]]" << '\n';
        out << "val=2" << '\n';
        out << "subsubcom.val=3" << '\n';
        out << "[[config]]" << '\n';
        out << "val=4" << '\n';
        out << "subsubcom.val=3" << '\n';
        out << "[[config]]" << '\n';
        out << "val=6" << '\n';
        out << "subsubcom.val=3" << '\n';
    }

    int val{0};
    app.add_option("--val", val);

    run();

    CHECK(2 == val);

    auto &index = app.get_config_formatter_base()->indexRef();
    index = 1;
    run();

    CHECK(4 == val);

    index = -1;
    run();
    // Take the first section in this case
    CHECK(2 == val);
    index = 2;
    run();

    CHECK(6 == val);
}

TEST_CASE_METHOD(TApp, "IniSubcommandConfigurableParseComplete", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "[subcom.subsubcom]" << '\n';
        out << "val=3" << '\n';
    }

    int one{0}, two{0}, three{0}, four{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    auto *subcom2 = app.add_subcommand("subcom2");
    subcom->configurable();
    std::vector<std::size_t> parse_c;
    subcom->parse_complete_callback([&parse_c]() { parse_c.push_back(58); });
    subcom->add_option("--val", two);
    subcom2->add_option("--val", four);
    subcom2->parse_complete_callback([&parse_c]() { parse_c.push_back(2623); });
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    // configurable should be inherited
    subsubcom->parse_complete_callback([&parse_c]() { parse_c.push_back(68); });
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);
    CHECK(four == 0);

    REQUIRE(2u == parse_c.size());
    CHECK(68U == parse_c[0]);
    CHECK(58U == parse_c[1]);
    CHECK(1u == subsubcom->count());
    CHECK(0u == subcom2->count());
}

TEST_CASE_METHOD(TApp, "IniSubcommandMultipleSections", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
        out << "[subcom]" << '\n';
        out << "val=2" << '\n';
        out << "[subcom.subsubcom]" << '\n';
        out << "val=3" << '\n';
        out << "[subcom2]" << '\n';
        out << "val=4" << '\n';
    }

    int one{0}, two{0}, three{0}, four{0};
    app.add_option("--val", one);
    auto *subcom = app.add_subcommand("subcom");
    auto *subcom2 = app.add_subcommand("subcom2");
    subcom->configurable();
    std::vector<std::size_t> parse_c;
    subcom->parse_complete_callback([&parse_c]() { parse_c.push_back(58); });
    subcom->add_option("--val", two);
    subcom2->add_option("--val", four);
    subcom2->parse_complete_callback([&parse_c]() { parse_c.push_back(2623); });
    subcom2->configurable(false);
    auto *subsubcom = subcom->add_subcommand("subsubcom");
    // configurable should be inherited
    subsubcom->parse_complete_callback([&parse_c]() { parse_c.push_back(68); });
    subsubcom->add_option("--val", three);

    run();

    CHECK(one == 1);
    CHECK(two == 2);
    CHECK(three == 3);
    CHECK(four == 4);

    REQUIRE(2u == parse_c.size());
    CHECK(68U == parse_c[0]);
    CHECK(58U == parse_c[1]);
    CHECK(1u == subsubcom->count());
    CHECK(0u == subcom2->count());
}

TEST_CASE_METHOD(TApp, "DuplicateSubcommandCallbacks", "[config]") {

    TempFile tmptoml{"TesttomlTmp.toml"};

    app.set_config("--config", tmptoml);

    {
        std::ofstream out{tmptoml};
        out << "[[foo]]" << '\n';
        out << "[[foo]]" << '\n';
        out << "[[foo]]" << '\n';
    }

    auto *foo = app.add_subcommand("foo");
    int count{0};
    foo->callback([&count]() { ++count; });
    foo->immediate_callback();
    CHECK(foo->get_immediate_callback());
    foo->configurable();

    run();
    CHECK(3 == count);
}

TEST_CASE_METHOD(TApp, "SubcommandCallbackSingle", "[config]") {

    TempFile tmptoml{"Testtomlcallback.toml"};

    app.set_config("--config", tmptoml);

    {
        std::ofstream out{tmptoml};
        out << "[foo]" << '\n';
    }
    int count{0};
    auto *foo = app.add_subcommand("foo");
    foo->configurable();
    foo->callback([&count]() { ++count; });

    run();
    CHECK(1 == count);
}

TEST_CASE_METHOD(TApp, "IniFailure", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.allow_config_extras(false);
    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::ConfigError);
}

TEST_CASE_METHOD(TApp, "IniConfigurable", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    bool value{false};
    app.add_flag("--val", value)->configurable(true);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(value);
}

TEST_CASE_METHOD(TApp, "IniNotConfigurable", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    bool value{false};
    app.add_flag("--val", value)->configurable(false);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=1" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::ConfigError);
    app.allow_config_extras(CLI::config_extras_mode::ignore_all);
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(TApp, "IniFlagDisableOverrideFlagArray", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    int value{0};
    app.add_flag("--val", value)->configurable(true)->disable_flag_override();

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=[1,true,false,true]" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(value == 2);
}

TEST_CASE_METHOD(TApp, "IniFlagInvalidDisableOverrideFlagArray", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    int value{0};
    app.add_flag("--val", value)->configurable(true)->disable_flag_override();

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "val=[1,true,false,not_valid]" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::InvalidError);
}

TEST_CASE_METHOD(TApp, "IniSubFailure", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.add_subcommand("other");
    app.set_config("--config", tmpini);
    app.allow_config_extras(false);
    {
        std::ofstream out{tmpini};
        out << "[other]" << '\n';
        out << "val=1" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::ConfigError);
}

TEST_CASE_METHOD(TApp, "IniNoSubFailure", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    app.allow_config_extras(CLI::config_extras_mode::error);
    {
        std::ofstream out{tmpini};
        out << "[other]" << '\n';
        out << "val=1" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::ConfigError);
}

TEST_CASE_METHOD(TApp, "IniFlagConvertFailure", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.add_flag("--flag");
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "flag=moobook" << '\n';
    }
    run();
    bool result{false};
    auto *opt = app.get_option("--flag");
    CHECK_THROWS_AS(opt->results(result), CLI::ConversionError);
    std::string res;
    opt->results(res);
    CHECK("moobook" == res);
}

TEST_CASE_METHOD(TApp, "IniFlagNumbers", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    bool boo{false};
    app.add_flag("--flag", boo);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "flag=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(boo);
}

TEST_CASE_METHOD(TApp, "IniFlagDual", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    bool boo{false};
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    app.add_flag("--flag", boo);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "flag=1 1" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "IniVectorMax", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    std::vector<std::string> v1;
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    app.add_option("--vec", v1)->expected(0, 2);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "vec=[a,b,c]" << '\n';
    }

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "IniShort", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "f=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniShortQuote1", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "\"f\"=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniShortQuote2", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "'f'=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniShortQuote3", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "`f`=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniDefaultPath", "[config]") {

    TempFile tmpini{"../TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    app.set_config("--config", "TestIniTmp.ini")->transform(CLI::FileOnDefaultPath("../"));

    {
        std::ofstream out{tmpini};
        out << "f=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniMultipleDefaultPath", "[config]") {

    TempFile tmpini{"../TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    auto *cfgOption = app.set_config("--config", "doesnotexist.ini")
                          ->transform(CLI::FileOnDefaultPath("../"))
                          ->transform(CLI::FileOnDefaultPath("../other", false));

    {
        std::ofstream out{tmpini};
        out << "f=3" << '\n';
    }

    args = {"--config", "TestIniTmp.ini"};
    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
    CHECK(cfgOption->as<std::string>() == "../TestIniTmp.ini");
}

TEST_CASE_METHOD(TApp, "IniMultipleDefaultPathAlternate", "[config]") {

    TempFile tmpini{"../TestIniTmp.ini"};

    int key{0};
    app.add_option("--flag,-f", key);
    auto *cfgOption = app.set_config("--config", "doesnotexist.ini")
                          ->transform(CLI::FileOnDefaultPath("../other") | CLI::FileOnDefaultPath("../"));

    {
        std::ofstream out{tmpini};
        out << "f=3" << '\n';
    }

    args = {"--config", "TestIniTmp.ini"};
    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
    CHECK(cfgOption->as<std::string>() == "../TestIniTmp.ini");
}

TEST_CASE_METHOD(TApp, "IniPositional", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    int key{0};
    app.add_option("key", key);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "key=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniEnvironmental", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    int key{0};
    app.add_option("key", key)->envname("CLI11_TEST_ENV_KEY_TMP");
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "CLI11_TEST_ENV_KEY_TMP=3" << '\n';
    }

    REQUIRE_NOTHROW(run());
    CHECK(3 == key);
}

TEST_CASE_METHOD(TApp, "IniFlagText", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    bool flag1{false}, flag2{false}, flag3{false}, flag4{false};
    app.add_flag("--flag1", flag1);
    app.add_flag("--flag2", flag2);
    app.add_flag("--flag3", flag3);
    app.add_flag("--flag4", flag4);
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "flag1=true" << '\n';
        out << "flag2=on" << '\n';
        out << "flag3=off" << '\n';
        out << "flag4=1" << '\n';
    }

    run();

    CHECK(flag1);
    CHECK(flag2);
    CHECK(!flag3);
    CHECK(flag4);
}

TEST_CASE_METHOD(TApp, "IniFlags", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2" << '\n';
        out << "three=true" << '\n';
        out << "four=on" << '\n';
        out << "five" << '\n';
    }

    int two{0};
    bool three{false}, four{false}, five{false};
    app.add_flag("--two", two);
    app.add_flag("--three", three);
    app.add_flag("--four", four);
    app.add_flag("--five", five);

    run();

    CHECK(two == 2);
    CHECK(three);
    CHECK(four);
    CHECK(five);
}

TEST_CASE_METHOD(TApp, "IniFlagsComment", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2 # comment 1" << '\n';
        out << "three=true" << '\n';
        out << "four=on #comment 2" << '\n';
        out << "five #comment 3" << '\n';
        out << '\n';
    }

    int two{0};
    bool three{false}, four{false}, five{false};
    app.add_flag("--two", two);
    app.add_flag("--three", three);
    app.add_flag("--four", four);
    app.add_flag("--five", five);

    run();

    CHECK(two == 2);
    CHECK(three);
    CHECK(four);
    CHECK(five);
}

TEST_CASE_METHOD(TApp, "IniFlagsAltComment", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2 % comment 1" << '\n';
        out << "three=true" << '\n';
        out << "four=on %% comment 2" << '\n';
        out << "five %= 3" << '\n';
        out << '\n';
    }

    auto config = app.get_config_formatter_base();
    config->comment('%');
    int two{0};
    bool three{false}, four{false}, five{false};
    app.add_flag("--two", two);
    app.add_flag("--three", three);
    app.add_flag("--four", four);
    app.add_flag("--five", five);

    run();

    CHECK(two == 2);
    CHECK(three);
    CHECK(four);
    CHECK(five);
}

TEST_CASE_METHOD(TApp, "IniFalseFlags", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=-2" << '\n';
        out << "three=false" << '\n';
        out << "four=1" << '\n';
        out << "five" << '\n';
    }

    int two{0};
    bool three{false}, four{false}, five{false};
    app.add_flag("--two", two);
    app.add_flag("--three", three);
    app.add_flag("--four", four);
    app.add_flag("--five", five);

    run();

    CHECK(two == -2);
    CHECK(!three);
    CHECK(four);
    CHECK(five);
}

TEST_CASE_METHOD(TApp, "IniFalseFlagsDef", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2" << '\n';
        out << "three=true" << '\n';
        out << "four=on" << '\n';
        out << "five" << '\n';
    }

    int two{0};
    bool three{false}, four{false}, five{false};
    app.add_flag("--two{false}", two);
    app.add_flag("--three", three);
    app.add_flag("!--four", four);
    app.add_flag("--five", five);

    run();

    CHECK(two == -2);
    CHECK(three);
    CHECK(!four);
    CHECK(five);
}

TEST_CASE_METHOD(TApp, "IniFalseFlagsDefDisableOverrideError", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2" << '\n';
        out << "four=on" << '\n';
        out << "five" << '\n';
    }

    int two{0};
    bool four{false}, five{false};
    app.add_flag("--two{false}", two)->disable_flag_override();
    app.add_flag("!--four", four);
    app.add_flag("--five", five);

    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);
}

TEST_CASE_METHOD(TApp, "IniFalseFlagsDefDisableOverrideSuccess", "[config]") {
    TempFile tmpini{"TestIniTmp.ini"};
    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2" << '\n';
        out << "four={}" << '\n';
        out << "val=15" << '\n';
    }

    int two{0}, four{0}, val{0};
    app.add_flag("--two{2}", two)->disable_flag_override();
    app.add_flag("--four{4}", four)->disable_flag_override();
    app.add_flag("--val", val);

    run();

    CHECK(two == 2);
    CHECK(four == 4);
    CHECK(val == 15);
}

static const int fclear3 = fileClear("TestIniTmp3.ini");

TEST_CASE_METHOD(TApp, "IniDisableFlagOverride", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    TempFile tmpini2{"TestIniTmp2.ini"};
    TempFile tmpini3{"TestIniTmp3.ini"};

    app.set_config("--config", tmpini);

    {
        std::ofstream out{tmpini};
        out << "[default]" << '\n';
        out << "two=2" << '\n';
    }

    {
        std::ofstream out{tmpini2};
        out << "[default]" << '\n';
        out << "two=7" << '\n';
    }

    {
        std::ofstream out{tmpini3};
        out << "[default]" << '\n';
        out << "three=true" << '\n';
    }

    int val{0};
    app.add_flag("--one{1},--two{2},--three{3}", val)->disable_flag_override();

    run();
    CHECK(tmpini.c_str() == app["--config"]->as<std::string>());
    CHECK(val == 2);

    args = {"--config", tmpini2};
    CHECK_THROWS_AS(run(), CLI::ArgumentMismatch);

    args = {"--config", tmpini3};
    run();

    CHECK(val == 3);
    CHECK(tmpini3.c_str() == app.get_config_ptr()->as<std::string>());
}

TEST_CASE("fclear", "[config]") {
    // mainly to clear up some warnings
    (void)fclear1;
    (void)fclear2;
    (void)fclear3;
}

TEST_CASE_METHOD(TApp, "TomlOutputSimple", "[config]") {

    int v{0};
    app.add_option("--simple", v);

    args = {"--simple=3"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "simple=3\n");
}

TEST_CASE_METHOD(TApp, "TomlOutputShort", "[config]") {

    int v{0};
    app.add_option("-s", v);

    args = {"-s3"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "s=3\n");
}

TEST_CASE_METHOD(TApp, "TomlOutputPositional", "[config]") {

    int v{0};
    app.add_option("pos", v);

    args = {"3"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "pos=3\n");
}

// try the output with environmental only arguments
TEST_CASE_METHOD(TApp, "TomlOutputEnvironmental", "[config]") {

    put_env("CLI11_TEST_ENV_TMP", "2");

    int val{1};
    app.add_option(std::string{}, val)->envname("CLI11_TEST_ENV_TMP");

    run();

    CHECK(val == 2);
    std::string str = app.config_to_str();
    CHECK(str == "CLI11_TEST_ENV_TMP=2\n");

    unset_env("CLI11_TEST_ENV_TMP");
}

TEST_CASE_METHOD(TApp, "TomlOutputNoConfigurable", "[config]") {

    int v1{0}, v2{0};
    app.add_option("--simple", v1);
    app.add_option("--noconf", v2)->configurable(false);

    args = {"--simple=3", "--noconf=2"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "simple=3\n");
}

TEST_CASE_METHOD(TApp, "TomlOutputShortSingleDescription", "[config]") {
    std::string flag = "some_flag";
    const std::string description = "Some short description.";
    app.add_flag("--" + flag, description);

    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("# " + description + "\n" + flag + "=false\n"));
}

TEST_CASE_METHOD(TApp, "TomlOutputdefaultOptionString", "[config]") {
    std::string option = "some_option";
    const std::string description = "Some short description.";
    app.add_option("--" + option, description)->run_callback_for_default();

    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("# " + description + "\n" + option + "=\"\"\n"));
}

TEST_CASE_METHOD(TApp, "TomlOutputShortDoubleDescription", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1);
    app.add_flag("--" + flag2, description2);

    run();

    std::string str = app.config_to_str(true, true);
    std::string ans = "# " + description1 + "\n" + flag1 + "=false\n\n# " + description2 + "\n" + flag2 + "=false\n";
    CHECK_THAT(str, Contains(ans));
}

TEST_CASE_METHOD(TApp, "TomlOutputGroups", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1)->group("group1");
    app.add_flag("--" + flag2, description2)->group("group2");

    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("group1"));
    CHECK_THAT(str, Contains("group2"));
}

TEST_CASE_METHOD(TApp, "TomlOutputHiddenOptions", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    double val{12.7};
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1)->group("group1");
    app.add_flag("--" + flag2, description2)->group("group2");
    app.add_option("--dval", val)->capture_default_str()->group("");

    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("group1"));
    CHECK_THAT(str, Contains("group2"));
    CHECK_THAT(str, Contains("dval=12.7"));
    auto loc = str.find("dval=12.7");
    auto locg1 = str.find("group1");
    CHECK(loc < locg1);
    // make sure it doesn't come twice
    loc = str.find("dval=12.7", loc + 4);
    CHECK(std::string::npos == loc);
}

TEST_CASE_METHOD(TApp, "TomlOutputAppMultiLineDescription", "[config]") {
    app.description("Some short app description.\n"
                    "That has multiple lines.");
    // for descriptions to show up needs an option that was set
    app.add_option("--test");
    args = {"--test", "55"};
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("# Some short app description.\n"));
    CHECK_THAT(str, Contains("# That has multiple lines.\n"));
}

TEST_CASE_METHOD(TApp, "TomlOutputMultiLineDescription", "[config]") {
    std::string flag = "some_flag";
    const std::string description = "Some short description.\nThat has lines.";
    app.add_flag("--" + flag, description);

    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("# Some short description.\n"));
    CHECK_THAT(str, Contains("# That has lines.\n"));
    CHECK_THAT(str, Contains(flag + "=false\n"));
}

TEST_CASE_METHOD(TApp, "TomlOutputOptionGroupMultiLineDescription", "[config]") {
    std::string flag = "flag";
    const std::string description = "Short flag description.\n";
    auto *og = app.add_option_group("group");
    og->description("Option group description.\n"
                    "That has multiple lines.");
    og->add_flag("--" + flag, description);
    args = {"--" + flag};
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("# Option group description.\n"));
    CHECK_THAT(str, Contains("# That has multiple lines.\n"));
}

TEST_CASE_METHOD(TApp, "TomlOutputMultilineString", "[config]") {
    std::string desc = "flag";
    app.add_option("--opt", desc);

    std::string argString = "this is a very long string \n that covers multiple lines \nand should be longer than 100 "
                            "characters \nto trigger the multiline string";
    args = {"--opt", argString};

    run();

    std::string str = app.config_to_str(true, true);

    std::istringstream nfile(str);

    app.clear();
    desc = "";
    app.parse_from_stream(nfile);
    CHECK(desc == argString);
}

TEST_CASE_METHOD(TApp, "TomlOutputSubcommandMultiLineDescription", "[config]") {
    std::string flag = "flag";
    const std::string description = "Short flag description.\n";
    auto *subcom = app.add_subcommand("subcommand");
    subcom->configurable();
    subcom->description("Subcommand description.\n"
                        "That has multiple lines.");
    subcom->add_flag("--" + flag, description);
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("# Subcommand description.\n"));
    CHECK_THAT(str, Contains("# That has multiple lines.\n"));
}

TEST_CASE_METHOD(TApp, "TomlOutputOptionGroup", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    double val{12.7};
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1)->group("group1");
    app.add_flag("--" + flag2, description2)->group("group2");
    auto *og = app.add_option_group("group3", "g3 desc");
    og->add_option("--dval", val)->capture_default_str()->group("");

    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("group1"));
    CHECK_THAT(str, Contains("group2"));
    CHECK_THAT(str, Contains("dval=12.7"));
    CHECK_THAT(str, Contains("group3"));
    CHECK_THAT(str, Contains("g3 desc"));
    auto loc = str.find("dval=12.7");
    auto locg1 = str.find("group1");
    auto locg3 = str.find("group3");
    CHECK(loc > locg1);
    // make sure it doesn't come twice
    loc = str.find("dval=12.7", loc + 4);
    CHECK(std::string::npos == loc);
    CHECK(locg1 < locg3);
}

TEST_CASE_METHOD(TApp, "TomlOutputVector", "[config]") {

    std::vector<int> v;
    app.add_option("--vector", v);
    app.config_formatter(std::make_shared<CLI::ConfigTOML>());
    args = {"--vector", "1", "2", "3"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "vector=[1, 2, 3]\n");
}

TEST_CASE_METHOD(TApp, "TomlOutputTuple", "[config]") {

    std::tuple<double, double, double, double> t;
    app.add_option("--tuple", t);
    app.config_formatter(std::make_shared<CLI::ConfigTOML>());
    args = {"--tuple", "1", "2", "3", "4"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "tuple=[1, 2, 3, 4]\n");
}

TEST_CASE_METHOD(TApp, "ConfigOutputVectorCustom", "[config]") {

    std::vector<int> v;
    app.add_option("--vector", v);
    auto V = std::make_shared<CLI::ConfigBase>();
    V->arrayBounds('{', '}')->arrayDelimiter(';')->valueSeparator(':');
    app.config_formatter(V);
    args = {"--vector", "1", "2", "3"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "vector:{1; 2; 3}\n");
}

TEST_CASE_METHOD(TApp, "TomlOutputFlag", "[config]") {

    int v{0}, q{0};
    app.add_option("--simple", v);
    app.add_flag("--nothing");
    app.add_flag("--onething");
    app.add_flag("--something", q);

    args = {"--simple=3", "--onething", "--something", "--something"};

    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=3"));
    CHECK_THAT(str, !Contains("nothing"));
    CHECK_THAT(str, Contains("onething=true"));
    CHECK_THAT(str, Contains("something=2"));

    str = app.config_to_str(true);
    CHECK_THAT(str, Contains("nothing"));
}

TEST_CASE_METHOD(TApp, "TomlOutputSet", "[config]") {

    int v{0};
    app.add_option("--simple", v)->check(CLI::IsMember({1, 2, 3}));

    args = {"--simple=2"};

    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=2"));
}

TEST_CASE_METHOD(TApp, "TomlOutputDefault", "[config]") {

    int v{7};
    app.add_option("--simple", v)->capture_default_str();

    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, !Contains("simple=7"));

    str = app.config_to_str(true);
    CHECK_THAT(str, Contains("simple=7"));

    app.get_config_formatter_base()->commentDefaults();
    str = app.config_to_str(true);
    CHECK_THAT(str, Contains("#simple=7"));
}

TEST_CASE_METHOD(TApp, "TomlOutputDefaultRequired", "[config]") {

    int v{7};
    auto *opt = app.add_option("--simple", v);
    opt->required()->run_callback_for_default(false);

    std::string str = app.config_to_str(true);
    CHECK_THAT(str, Contains("simple=\"<REQUIRED>\""));

    opt->required(false);
    str = app.config_to_str(true);
    CHECK_THAT(str, Contains("simple=\"\""));
}

TEST_CASE_METHOD(TApp, "TomlOutputSubcom", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");

    args = {"--simple", "other", "--newer"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other.newer=true"));
}

TEST_CASE_METHOD(TApp, "TomlOutputSubcomConfigurable", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other")->configurable();
    subcom->add_flag("--newer");

    args = {"--simple", "other", "--newer"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other]"));
    CHECK_THAT(str, Contains("newer=true"));
    CHECK(std::string::npos == str.find("other.newer=true"));
}

TEST_CASE_METHOD(TApp, "TomlOutputSubsubcom", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");

    args = {"--simple", "other", "--newer", "sub2", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other.newer=true"));
    CHECK_THAT(str, Contains("other.sub2.newest=true"));
}

TEST_CASE_METHOD(TApp, "TomlOutputSubsubcomConfigurable", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other")->configurable();
    subcom->add_flag("--newer");

    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");

    args = {"--simple", "other", "--newer", "sub2", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other]"));
    CHECK_THAT(str, Contains("newer=true"));
    CHECK_THAT(str, Contains("[other.sub2]"));
    CHECK_THAT(str, Contains("newest=true"));
    CHECK(std::string::npos == str.find("sub2.newest=true"));
}

TEST_CASE_METHOD(TApp, "TomlOutputSubcomNonConfigurable", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other", "other_descriptor")->configurable();
    subcom->add_flag("--newer");

    auto *subcom2 = app.add_subcommand("sub2", "descriptor2");
    subcom2->add_flag("--newest")->configurable(false);

    args = {"--simple", "other", "--newer", "sub2", "--newest"};
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("other_descriptor"));
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other]"));
    CHECK_THAT(str, Contains("newer=true"));
    CHECK_THAT(str, !Contains("newest"));
    CHECK_THAT(str, !Contains("descriptor2"));
}

TEST_CASE_METHOD(TApp, "TomlOutputSubsubcomConfigurableDeep", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other")->configurable();
    subcom->add_flag("--newer");

    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");
    auto *sssscom = subsubcom->add_subcommand("sub-level2");
    subsubcom->add_flag("--still_newer");
    auto *s5com = sssscom->add_subcommand("sub-level3");
    s5com->add_flag("--absolute_newest");

    args = {"--simple", "other", "sub2", "sub-level2", "sub-level3", "--absolute_newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other.sub2.sub-level2.sub-level3]"));
    CHECK_THAT(str, Contains("absolute_newest=true"));
    CHECK(std::string::npos == str.find(".absolute_newest=true"));
}

TEST_CASE_METHOD(TApp, "TomlOutputQuoted", "[config]") {

    std::string val1;
    app.add_option("--val1", val1);

    std::string val2;
    app.add_option("--val2", val2);

    args = {"--val1", "I am a string", "--val2", R"(I am a "confusing" string)"};

    run();

    CHECK(val1 == "I am a string");
    CHECK(val2 == "I am a \"confusing\" string");

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("val1=\"I am a string\""));
    CHECK_THAT(str, Contains("val2=\"I am a \\\"confusing\\\" string\""));
}

TEST_CASE_METHOD(TApp, "DefaultsTomlOutputQuoted", "[config]") {

    std::string val1{"I am a string"};
    app.add_option("--val1", val1)->capture_default_str();

    std::string val2{R"(I am a "confusing" string)"};
    app.add_option("--val2", val2)->capture_default_str();

    run();

    std::string str = app.config_to_str(true);
    CHECK_THAT(str, Contains("val1=\"I am a string\""));
    CHECK_THAT(str, Contains("val2=\"I am a \\\"confusing\\\" string\""));
}

// #298
TEST_CASE_METHOD(TApp, "StopReadingConfigOnClear", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.set_config("--config", tmpini);
    auto *ptr = app.set_config();  // Should *not* read config file
    CHECK(nullptr == ptr);

    {
        std::ofstream out{tmpini};
        out << "volume=1" << '\n';
    }

    int volume{0};
    app.add_option("--volume", volume, "volume1");

    run();

    CHECK(0 == volume);
}

TEST_CASE_METHOD(TApp, "ConfigWriteReadWrite", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};

    app.add_flag("--flag");
    run();

    // Save config, with default values too
    std::string config1 = app.config_to_str(true, true);
    {
        std::ofstream out{tmpini};
        out << config1 << '\n';
    }

    app.set_config("--config", tmpini, "Read an ini file", true);
    run();

    std::string config2 = app.config_to_str(true, true);

    CHECK(config2 == config1);
}

TEST_CASE_METHOD(TApp, "ConfigWriteReadNegated", "[config]") {

    TempFile tmpini{"TestIniTmp.ini"};
    bool flag{true};
    app.add_flag("!--no-flag", flag);
    args = {"--no-flag"};
    run();

    // Save config, with default values too
    std::string config1 = app.config_to_str(false, false);
    {
        std::ofstream out{tmpini};
        out << config1 << '\n';
    }
    CHECK_FALSE(flag);
    args.clear();
    flag = true;
    app.set_config("--config", tmpini, "Read an ini file", true);
    run();

    CHECK_FALSE(flag);
}

/////// INI output tests

TEST_CASE_METHOD(TApp, "IniOutputSimple", "[config]") {

    int v{0};
    app.add_option("--simple", v);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple=3"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "simple=3\n");
}

TEST_CASE_METHOD(TApp, "IniOutputNoConfigurable", "[config]") {

    int v1{0}, v2{0};
    app.add_option("--simple", v1);
    app.add_option("--noconf", v2)->configurable(false);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple=3", "--noconf=2"};

    run();

    std::string str = app.config_to_str();
    CHECK(str == "simple=3\n");
}

TEST_CASE_METHOD(TApp, "IniOutputShortSingleDescription", "[config]") {
    std::string flag = "some_flag";
    const std::string description = "Some short description.";
    app.add_flag("--" + flag, description);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("; " + description + "\n" + flag + "=false\n"));
}

TEST_CASE_METHOD(TApp, "IniOutputShortDoubleDescription", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1);
    app.add_flag("--" + flag2, description2);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    std::string ans = "; " + description1 + "\n" + flag1 + "=false\n\n; " + description2 + "\n" + flag2 + "=false\n";
    CHECK_THAT(str, Contains(ans));
}

TEST_CASE_METHOD(TApp, "IniOutputGroups", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1)->group("group1");
    app.add_flag("--" + flag2, description2)->group("group2");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("group1"));
    CHECK_THAT(str, Contains("group2"));
}

TEST_CASE_METHOD(TApp, "IniOutputHiddenOptions", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    double val{12.7};
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1)->group("group1");
    app.add_flag("--" + flag2, description2)->group("group2");
    app.add_option("--dval", val)->capture_default_str()->group("");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("group1"));
    CHECK_THAT(str, Contains("group2"));
    CHECK_THAT(str, Contains("dval=12.7"));
    auto loc = str.find("dval=12.7");
    auto locg1 = str.find("group1");
    CHECK(loc < locg1);
    // make sure it doesn't come twice
    loc = str.find("dval=12.7", loc + 4);
    CHECK(std::string::npos == loc);
}

TEST_CASE_METHOD(TApp, "IniOutputAppMultiLineDescription", "[config]") {
    app.description("Some short app description.\n"
                    "That has multiple lines.");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());

    // for descriptions to show up needs an option that was set
    app.add_option("--test");
    args = {"--test", "66"};
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("; Some short app description.\n"));
    CHECK_THAT(str, Contains("; That has multiple lines.\n"));
}

TEST_CASE_METHOD(TApp, "IniOutputMultiLineDescription", "[config]") {
    std::string flag = "some_flag";
    const std::string description = "Some short description.\nThat has lines.";
    app.add_flag("--" + flag, description);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("; Some short description.\n"));
    CHECK_THAT(str, Contains("; That has lines.\n"));
    CHECK_THAT(str, Contains(flag + "=false\n"));
}

TEST_CASE_METHOD(TApp, "IniOutputOptionGroupMultiLineDescription", "[config]") {
    std::string flag = "flag";
    const std::string description = "Short flag description.\n";
    auto *og = app.add_option_group("group");
    og->description("Option group description.\n"
                    "That has multiple lines.");
    og->add_flag("--" + flag, description);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("; Option group description.\n"));
    CHECK_THAT(str, Contains("; That has multiple lines.\n"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubcommandMultiLineDescription", "[config]") {
    std::string flag = "flag";
    const std::string description = "Short flag description.\n";
    auto *subcom = app.add_subcommand("subcommand");
    subcom->configurable();
    subcom->description("Subcommand description.\n"
                        "That has multiple lines.");
    subcom->add_flag("--" + flag, description);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("; Subcommand description.\n"));
    CHECK_THAT(str, Contains("; That has multiple lines.\n"));
}

TEST_CASE_METHOD(TApp, "IniOutputOptionGroup", "[config]") {
    std::string flag1 = "flagnr1";
    std::string flag2 = "flagnr2";
    double val{12.7};
    const std::string description1 = "First description.";
    const std::string description2 = "Second description.";
    app.add_flag("--" + flag1, description1)->group("group1");
    app.add_flag("--" + flag2, description2)->group("group2");
    auto *og = app.add_option_group("group3", "g3 desc");
    og->add_option("--dval", val)->capture_default_str()->group("");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true, true);
    CHECK_THAT(str, Contains("group1"));
    CHECK_THAT(str, Contains("group2"));
    CHECK_THAT(str, Contains("dval=12.7"));
    CHECK_THAT(str, Contains("group3"));
    CHECK_THAT(str, Contains("g3 desc"));
    auto loc = str.find("dval=12.7");
    auto locg1 = str.find("group1");
    auto locg3 = str.find("group3");
    CHECK(loc > locg1);
    // make sure it doesn't come twice
    loc = str.find("dval=12.7", loc + 4);
    CHECK(std::string::npos == loc);
    CHECK(locg1 < locg3);
}

TEST_CASE_METHOD(TApp, "IniOutputVector", "[config]") {

    std::vector<int> v;
    app.add_option("--vector", v);

    args = {"--vector", "1", "2", "3"};
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str();
    CHECK(str == "vector=1 2 3\n");
}

TEST_CASE_METHOD(TApp, "IniOutputFlag", "[config]") {

    int v{0}, q{0};
    app.add_option("--simple", v);
    app.add_flag("--nothing");
    app.add_flag("--onething");
    app.add_flag("--something", q);

    args = {"--simple=3", "--onething", "--something", "--something"};
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=3"));
    CHECK_THAT(str, !Contains("nothing"));
    CHECK_THAT(str, Contains("onething=true"));
    CHECK_THAT(str, Contains("something=2"));

    str = app.config_to_str(true);
    CHECK_THAT(str, Contains("nothing"));
}

TEST_CASE_METHOD(TApp, "IniOutputSet", "[config]") {

    int v{0};
    app.add_option("--simple", v)->check(CLI::IsMember({1, 2, 3}));

    args = {"--simple=2"};
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=2"));
}

TEST_CASE_METHOD(TApp, "IniOutputDefault", "[config]") {

    int v{7};
    app.add_option("--simple", v)->capture_default_str();
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, !Contains("simple=7"));

    str = app.config_to_str(true);
    CHECK_THAT(str, Contains("simple=7"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubcom", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple", "other", "--newer"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other.newer=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubcomCustomSep", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    app.get_config_formatter_base()->parentSeparator(':');
    args = {"--simple", "other", "--newer"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other:newer=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubcomConfigurable", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other")->configurable();
    subcom->add_flag("--newer");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple", "other", "--newer"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other]"));
    CHECK_THAT(str, Contains("newer=true"));
    CHECK(std::string::npos == str.find("other.newer=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcom", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple", "other", "--newer", "sub2", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other.newer=true"));
    CHECK_THAT(str, Contains("other.sub2.newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcomWithDot", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    auto *subsubcom = subcom->add_subcommand("sub2.bb");
    subsubcom->add_flag("--newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple", "other", "--newer", "sub2.bb", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other.newer=true"));
    CHECK_THAT(str, Contains("other.'sub2.bb'.newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcomCustomSep", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    app.get_config_formatter_base()->parentSeparator('|');
    args = {"--simple", "other", "--newer", "sub2", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other|newer=true"));
    CHECK_THAT(str, Contains("other|sub2|newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcomCustomSepWithInternalSep", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    auto *subsubcom = subcom->add_subcommand("sub2|BB");
    subsubcom->add_flag("--newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    app.get_config_formatter_base()->parentSeparator('|');
    args = {"--simple", "other", "--newer", "sub2|BB", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other|newer=true"));
    CHECK_THAT(str, Contains("other|'sub2|BB'|newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcomCustomSepWithInternalQuote", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other");
    subcom->add_flag("--newer");
    auto *subsubcom = subcom->add_subcommand("sub2'BB");
    subsubcom->add_flag("--newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    app.get_config_formatter_base()->parentSeparator('|');
    args = {"--simple", "other", "--newer", "sub2'BB", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("other|newer=true"));
    CHECK_THAT(str, Contains("other|\"sub2'BB\"|newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcomConfigurable", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other")->configurable();
    subcom->add_flag("--newer");

    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple", "other", "--newer", "sub2", "--newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other]"));
    CHECK_THAT(str, Contains("newer=true"));
    CHECK_THAT(str, Contains("[other.sub2]"));
    CHECK_THAT(str, Contains("newest=true"));
    CHECK(std::string::npos == str.find("sub2.newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputSubsubcomConfigurableDeep", "[config]") {

    app.add_flag("--simple");
    auto *subcom = app.add_subcommand("other")->configurable();
    subcom->add_flag("--newer");

    auto *subsubcom = subcom->add_subcommand("sub2");
    subsubcom->add_flag("--newest");
    auto *sssscom = subsubcom->add_subcommand("sub-level2");
    subsubcom->add_flag("--still_newer");
    auto *s5com = sssscom->add_subcommand("sub-level3");
    s5com->add_flag("--absolute_newest");
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--simple", "other", "sub2", "sub-level2", "sub-level3", "--absolute_newest"};
    run();

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("simple=true"));
    CHECK_THAT(str, Contains("[other.sub2.sub-level2.sub-level3]"));
    CHECK_THAT(str, Contains("absolute_newest=true"));
    CHECK(std::string::npos == str.find(".absolute_newest=true"));
}

TEST_CASE_METHOD(TApp, "IniOutputQuoted", "[config]") {

    std::string val1;
    app.add_option("--val1", val1);

    std::string val2;
    app.add_option("--val2", val2);
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    args = {"--val1", "I am a string", "--val2", R"(I am a "confusing" string)"};

    run();

    CHECK(val1 == "I am a string");
    CHECK(val2 == "I am a \"confusing\" string");

    std::string str = app.config_to_str();
    CHECK_THAT(str, Contains("val1=\"I am a string\""));
    CHECK_THAT(str, Contains("val2=\"I am a \\\"confusing\\\" string\""));
}

TEST_CASE_METHOD(TApp, "DefaultsIniOutputQuoted", "[config]") {

    std::string val1{"I am a string"};
    app.add_option("--val1", val1)->capture_default_str();

    std::string val2{R"(I am a "confusing" string)"};
    app.add_option("--val2", val2)->capture_default_str();
    app.config_formatter(std::make_shared<CLI::ConfigINI>());
    run();

    std::string str = app.config_to_str(true);
    CHECK_THAT(str, Contains("val1=\"I am a string\""));
    CHECK_THAT(str, Contains("val2=\"I am a \\\"confusing\\\" string\""));
}

TEST_CASE_METHOD(TApp, "RoundTripEmptyVector", "[config]") {
    std::vector<std::string> cv{};
    app.add_option("-c", cv)->expected(0, 2);

    args = {"-c", "{}"};

    run();
    CHECK(cv.empty());
    cv.clear();
    std::string configOut = app.config_to_str();
    app.clear();
    std::stringstream out(configOut);
    app.parse_from_stream(out);
    CHECK(cv.empty());
}
